#include "EventProcessor.hxx"
#include "BdeMessage.hxx"
#include "RunLoopScheduler.hxx"
#include "database/CommitGuard.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "exporter/client/EpaMedicationClientImpl.hxx"
#include "exporter/database/MedicationExporterDatabaseFrontend.hxx"
#include "exporter/eventprocessing/EventDispatcher.hxx"
#include "exporter/model/DeviceId.hxx"
#include "exporter/model/EventKvnr.hxx"
#include "exporter/model/TaskEvent.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/audit/AuditDataCollector.hxx"
#include "shared/database/AccessTokenIdentity.hxx"
#include "shared/database/TransactionMode.hxx"
#include "shared/model/GemErpEuPrMedicationDispense.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Demangle.hxx"
#include "shared/util/JsonLog.hxx"
#include "util/RuntimeConfiguration.hxx"

using namespace std::chrono_literals;

template<typename FuncT>
decltype(auto) EventProcessor::autocommit(FuncT&& function)
{
    return mServiceContext->transaction(TransactionMode::autocommit, std::forward<FuncT>(function));
}

EventProcessor::EventProcessor(const std::shared_ptr<MedicationExporterServiceContext>& serviceContext, IEpaAccountLookup& epaAccountLookup)
    : jsonLog([] {
        JsonLog jlog(LogId::INFO, JsonLog::makeInfoLogReceiver(), false);
        if (tlogContext.has_value())
        {
            jlog << KeyValue("x-request-id", *tlogContext);
        }
        return jlog;
    })
    , mServiceContext(serviceContext)
    , mEpaAccountLookup(epaAccountLookup)
    , mRetryDelaySeconds(
          Configuration::instance().getIntValue(ConfigurationKey::MEDICATION_EXPORTER_RETRIES_RESCHEDULE_DELAY_SECONDS))
    , mHealthRecordRelocationWaitMinutes(
          Configuration::instance().getIntValue(ConfigurationKey::MEDICATION_EXPORTER_EPA_CONFLICT_WAIT_MINUTES))
    , mMaxRetryAttempts(Configuration::instance().getIntValue(
          ConfigurationKey::MEDICATION_EXPORTER_RETRIES_MAXIMUM_BEFORE_DEADLETTER))
{
}

bool EventProcessor::process()
{
    // Workflow step 1
    const auto kvnr = autocommit([&](auto& db) {
        DurationConsumerGuard durationConsumerGuard{
            "none", mServiceContext->getRuntimeConfigurationGetter()->getMetricsLogThresholdsMs()};
        return db.processNextKvnr();
    });

    if (kvnr)
    {
        try
        {
            processOne(*kvnr);
        }
        catch (const db_model::AccessTokenIdentity::Exception& e)
        {
            jsonLog() << *kvnr << KeyValue("event", "Processing task events: Unexpected error. Mark first event as deadletter")
                      << KeyValue("reason", e.what());
            autocommit([&](auto& db) {
                db.markFirstEventDeadLetter(*kvnr);
            });
        }
        catch (const std::exception& e)
        {
            std::string reason = dynamic_cast<const model::ModelException*>(&e) ? "not given" : e.what();
            jsonLog().locationFromException(e)
                << *kvnr << KeyValue("event", "Processing task events: Unexpected error. Rescheduling KVNR")
                << KeyValue("retryCount", std::to_string(kvnr->getRetryCount())) << KeyValue("reason", reason);
            scheduleRetryQueue(*kvnr);
        }
        return true;
    }
    // Nothing processed.
    return false;
}

void EventProcessor::processOne(const model::EventKvnr& kvnr)
{
    // Workflow step 18
    if (checkRetryCount(kvnr))
    {
        // Workflow step 2
        auto events = autocommit([&](auto& db) {
            return db.getAllEventsForKvnr(kvnr);
        });

        jsonLog() << kvnr << KeyValue("retryCount", std::to_string(kvnr.getRetryCount()))
                  << KeyValue("event", "Start Processing KVNR with " + std::to_string(events.size()) + " events");

        removeEuMedicationDispenseEvents(kvnr, events);

        if (! events.empty())
        {
            const auto& firstEvent = *events.front();
            ScopedLogContext scopedLogContext{firstEvent.getXRequestId()};

            auto durationConsumerGuard{std::make_unique<DurationConsumerGuard>(
                firstEvent.getXRequestId(),
                mServiceContext->getRuntimeConfigurationGetter()->getMetricsLogThresholdsMs())};
            // Workflow step 3 - decodedKvnr = decrypt(events.front()).kvnr
            auto epaAccount = ePaAccountLookup(kvnr, firstEvent);
            durationConsumerGuard.reset();

            // Workflow step 4 - ePA account lookup
            switch (epaAccount.lookupResult)
            {
                case EpaAccount::Code::allowed:
                    // Workflow step 4a
                    TVLOG(1) << "account lookup allowed with given kvnr";
                    checkDeactivateThrottle(epaAccount.host);
                    processEpaAllowed(kvnr, epaAccount, events);
                    break;
                case EpaAccount::Code::deny:
                    [[fallthrough]];
                case EpaAccount::Code::notFound:
                    // Workflow step 4b - deny / not found -> Workflow step 14
                    checkDeactivateThrottle(epaAccount.host);
                    processEpaDeniedOrNotFound(kvnr);
                    break;
                case EpaAccount::Code::conflict:
                    // Workflow step 4c
                    checkDeactivateThrottle(epaAccount.host);
                    processEpaConflict(kvnr);
                    break;
                case EpaAccount::Code::unknown:
                    // Workflow step 4d
                    TVLOG(1) << "account lookup unknown with given kvnr";
                    mergeFailingEpas(std::move(epaAccount.failingHosts));
                    processEpaUnknown(kvnr);
                    break;
            }
        }
        else
        {
            // Workflow step 13 - set kvnr to processed
            autocommit([&](auto& db) {
                A_25941.start("Remove cached prefix, no more events for kvnr");
                db.finalizeKvnr(kvnr, "");
                A_25941.finish();
            });
            TLOG(WARNING) << "KVNR to process had no events";
        }
    }
}

bool EventProcessor::checkRetryCount(const model::EventKvnr& kvnr)
{
    const auto retry = kvnr.getRetryCount();
    // Workflow step 18 check retry_count
    if (retry > mMaxRetryAttempts)
    {
        AuditDataCollector auditDataCollector;
        auditDataCollector.setAgentName("E-Rezept-Fachdienst");
        auditDataCollector.setAgentType(model::AuditEvent::AgentType::machine);
        auditDataCollector.setAgentWho("9-E-Rezept-Fachdienst");
        auditDataCollector.setDeviceId(ExporterDeviceId);

        // Workflow step 19 mark first event as deadletter-queue
        if (const auto unexpandedTaskEvent = autocommit([&](MedicationExporterDatabaseFrontendInterface& db) {
                return db.markFirstEventDeadLetter(kvnr);
            }))
        {
            auditDataCollector.setPrescriptionId(unexpandedTaskEvent->prescriptionId);
            auditDataCollector.setInsurantKvnr(unexpandedTaskEvent->kvnr);

            switch (unexpandedTaskEvent->useCase)
            {
                case model::TaskEvent::UseCase::providePrescription:
                    A_25962.start("Die Verordnung konnte nicht in die Patientenakte übertragen werden.");
                    auditDataCollector.setAction(model::AuditEvent::Action::create);
                    auditDataCollector.setEventId(model::AuditEventId::POST_PROVIDE_PRESCRIPTION_ERP_failed);
                    A_25962.finish();
                    break;
                case model::TaskEvent::UseCase::cancelPrescription:
                    A_25962.start(
                        "Die Löschinformation zum E-Rezept konnte nicht in die Patientenakte übermittelt werden.");
                    auditDataCollector.setAction(model::AuditEvent::Action::del);
                    auditDataCollector.setEventId(model::AuditEventId::POST_CANCEL_PRESCRIPTION_ERP_failed);
                    A_25962.finish();
                    break;
                case model::TaskEvent::UseCase::provideDispensation:
                    A_25962.start("Die Medikamentenabgabe konnte nicht in die Patientenakte übertragen werden.");
                    auditDataCollector.setAction(model::AuditEvent::Action::create);
                    auditDataCollector.setEventId(model::AuditEventId::POST_PROVIDE_DISPENSATION_ERP_failed);
                    A_25962.finish();
                    break;
                case model::TaskEvent::UseCase::cancelDispensation:// unreachable
                    break;
            }
            jsonLog() << kvnr
                      << KeyValue("event", "Put first event for task in dead letter queue")
                      << KeyValue("retryCount", std::to_string(kvnr.getRetryCount()))
                      << KeyValue("prescription_id", unexpandedTaskEvent->prescriptionId.toString())
                      << KeyValue("reason", "Max retry count reached");
            // Workflow step 20 write audit event
            writeAuditEvent(auditDataCollector);
        }
        else
        {
             jsonLog() << kvnr
                      << KeyValue("event", "KVNR has exceeded retry_count but events to put on deadlettter queue")
                      << KeyValue("retryCount", std::to_string(kvnr.getRetryCount()))
                      << KeyValue("reason", "Max retry count reached");
        }

        // Workflow step 21 set KVNR to pending, retry_count to 0 and reschedule in delay-seconds
        autocommit([&](auto& db) {
            db.updateProcessingDelay(0, mRetryDelaySeconds, kvnr);
        });
        return false;
    }
    return true;
}

void EventProcessor::processEpaAllowed(const model::EventKvnr& kvnr, EpaAccount& epaAccount,
                                       const MedicationExporterDatabaseFrontendInterface::taskevents_t& events)
{
    AuditDataCollector auditDataCollector;
    auto medicationClient = std::make_unique<EpaMedicationClientImpl>(mServiceContext->ioContext(), epaAccount.host,
                                                                      mServiceContext->teeClientPool());
    auto dispatcher = eventprocessing::EventDispatcher{std::move(medicationClient)};
    // Workflow step 5 - iterate over all events
    for (const auto& event : events)
    {
        ScopedLogContext scopedLogContext{event->getXRequestId()};
        DurationConsumerGuard durationConsumerGuard{
            event->getXRequestId(), mServiceContext->getRuntimeConfigurationGetter()->getMetricsLogThresholdsMs()};
        Expect(event, "No or bad events for kvnr");

        // Workflow step 6 - check deadletter
        bool deadLetter = autocommit([&](auto& db) {
            return db.isDeadLetter(kvnr, event->getPrescriptionId(), event->getPrescriptionType());
        });
        // Workflow step 6b - if event in deadletterQueue -> Workflow step 17
        if (deadLetter)
        {
            // Workflow step 17 - set deadletter
            const auto skipped = autocommit([&](auto& db) {
                return db.markDeadLetter(kvnr, event->getPrescriptionId(), event->getPrescriptionType());
            });
            TVLOG(1) << "marked events as deadletterQueue: " << skipped;
            jsonLog() << kvnr << KeyValue("event", "Setting all events for task in dead letter queue")
                      << KeyValue("retryCount", std::to_string(kvnr.getRetryCount()))
                      << KeyValue("prescription_id", event->getPrescriptionId().toString());
            continue;
        }

        // Workflow step 7 - check usecase of event (and execute step 8,9 or 10)
        jsonLog() << kvnr << KeyValue("event", "Processing event")
                  << KeyValue("prescription_id", event->getPrescriptionId().toString())
                  << KeyValue("usecase", magic_enum::enum_name(event->getUseCase()))
                  << KeyValue("epa", epaAccount.host);
        auto outcome = dispatcher.dispatch(*event, auditDataCollector);

        A_25962.start("Write audit event to database");
        if (outcome == eventprocessing::Outcome::Success || outcome == eventprocessing::Outcome::DeadLetter)
        {
            writeAuditEvent(auditDataCollector);
        }
        A_25962.finish();

        // Workflow step 11 - check ePA response
        switch (outcome)
        {
            case eventprocessing::Outcome::Success:
            case eventprocessing::Outcome::SuccessAuditFail: {// Workflow step 11a - Success -> Workflow step 15
                // Workflow step 12 - delete successfully transmitted event
                jsonLog() << kvnr << KeyValue("event", "Deleting event after successful transmission")
                          << KeyValue("prescription_id", event->getPrescriptionId().toString());
                autocommit([&](auto& db) {
                    db.deleteOneEventForKvnr(kvnr, event->getId());
                });
            }
            break;
            case eventprocessing::Outcome::Retry:// Workflow step 11c - RetryQueue -> Workflow step 15
                // Workflow step 15 - set kvnr to pending and reschedule it
                {
                    jsonLog() << kvnr << KeyValue("retryCount", std::to_string(kvnr.getRetryCount()))
                              << KeyValue("event", "Technical error occurred. Rescheduling KVNR");
                    scheduleRetryQueue(kvnr);
                    return;// break transmission of remaining events of this KVNR
                }
                break;
            case eventprocessing::Outcome::Conflict:
                processEpaConflict(kvnr);
                break;
            case eventprocessing::Outcome::ConsentRevoked:
                processEpaDeniedOrNotFound(kvnr);
                break;
            case eventprocessing::Outcome::DeadLetter:// Workflow step 11b - DeadletterQueue -> Workflow step 17
                // Workflow step 17 - set deadletter
                const auto skipped = autocommit([&](auto& db) {
                    return db.markDeadLetter(kvnr, event->getPrescriptionId(), event->getPrescriptionType());
                });
                TVLOG(1) << "marked events as deadletterQueue: " << skipped;
                jsonLog() << kvnr << KeyValue("event", "Setting all events for task in dead letter queue")
                          << KeyValue("retryCount", std::to_string(kvnr.getRetryCount()))
                          << KeyValue("prescription_id", event->getPrescriptionId().toString());
                break;
        }
    }
    // Workflow step 13 - set kvnr to processed
    autocommit([&](auto& db) {
        A_25941.start("Store prefix for reuse.");
        db.finalizeKvnr(kvnr, epaAccount.host);
        A_25941.finish();
    });
    jsonLog() << kvnr << KeyValue("event", "KVNR processed");
}

void EventProcessor::processEpaDeniedOrNotFound(const model::EventKvnr& kvnr)
{
    TVLOG(1) << "account lookup denied / not found with given kvnr";

    // Workflow step 14 - delete kvnr events
    autocommit([&](auto& db) {
        db.deleteAllEventsForKvnr(kvnr);
    });

    jsonLog() << kvnr << KeyValue("event", "Delete all Task Events for KVNR");

    // Workflow step 13 - set kvnr to processed
    autocommit([&](auto& db) {
        A_25941.start("Remove cached prefix for lookup denied / not found");
        db.finalizeKvnr(kvnr, "");
        A_25941.finish();
    });
}

void EventProcessor::processEpaConflict(const model::EventKvnr& kvnr)
{
    TVLOG(1) << "account lookup conflict with given kvnr";
    scheduleHealthRecordRelocation(kvnr);
    jsonLog() << kvnr
              << KeyValue("event",
                          "Information service responded with conflict (Aktenumzug) for KVNR. Reschedule KVNR in 24h.");
}

void EventProcessor::processEpaUnknown(const model::EventKvnr& kvnr)
{
    jsonLog() << kvnr << KeyValue("retryCount", std::to_string(kvnr.getRetryCount()))
              << KeyValue("event", "Technical error occurred. Rescheduling KVNR");
    auto configSetter = mServiceContext->getRuntimeConfigurationSetter();
    switch (configSetter->throttleMode())
    {
        case exporter::RuntimeConfiguration::ThrottleMode::MANUAL:
            if (configSetter->throttleValue() > 0ms)
            {
                // don't change
                break;
            }
            [[fallthrough]];
        case exporter::RuntimeConfiguration::ThrottleMode::AUTOMATIC_EPA_LOOKUP:
            A_25942.start("enable throttling when lookup fails, currently no exponential backoff for simplicity");
            configSetter->throttle(exporter::RuntimeConfiguration::ThrottleMode::AUTOMATIC_EPA_LOOKUP,
                                   Configuration::instance().getIntValue(
                                       ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_THROTTLE_SECONDS) *
                                       1s);
            break;
    }
    scheduleRetryQueue(kvnr);
}

void EventProcessor::writeAuditEvent(const AuditDataCollector& auditDataCollector)
{
    try
    {
        model::AuditData auditData = auditDataCollector.createData();
        CommitGuard{mServiceContext->erpDatabaseFactory()}.db().storeAuditEventData(auditData);
    }
    catch (const MissingAuditDataException& exc)
    {
        TVLOG(2) << "Missing audit data";
        JsonLog(LogId::INFO, JsonLog::makeErrorLogReceiver(), false).details("Missing audit data");
    }
    catch (const std::exception& exc)
    {
        // Could be an I/O error.
        const auto typeinfo = util::demangle(typeid(exc).name());
        TVLOG(1) << "Error while storing audit data: " << typeinfo;
        TVLOG(1) << "Error reason:  " << exc.what();
    }
}

void EventProcessor::mergeFailingEpas(std::set<std::string>&& failingEpas)
{
    const std::unique_lock lock(mFailingEpasMutex);
    mFailingEpas.merge(std::move(failingEpas));
}

void EventProcessor::checkDeactivateThrottle(const std::string& hostNotFailing)
{
    TVLOG(1) << "checkDeactivateThrottle: " << hostNotFailing;
    const std::unique_lock lock(mFailingEpasMutex);
    if (mFailingEpas.empty())
    {
        return;
    }
    mFailingEpas.erase(hostNotFailing);
    if (mFailingEpas.empty())
    {
        auto configSetter = mServiceContext->getRuntimeConfigurationSetter();
        switch (configSetter->throttleMode())
        {
            case exporter::RuntimeConfiguration::ThrottleMode::MANUAL:
                // don't change
                break;
            case exporter::RuntimeConfiguration::ThrottleMode::AUTOMATIC_EPA_LOOKUP:
                A_25942.start("disable throttling when lookup succeeds for all EPAs");
                configSetter->throttle(exporter::RuntimeConfiguration::ThrottleMode::AUTOMATIC_EPA_LOOKUP, 0ms);
                break;
        }
    }
}

EpaAccount EventProcessor::ePaAccountLookup(const model::EventKvnr& kvnr, const model::TaskEvent& taskEvent)
{
    BDEMessage::Data data;
    data.lastModified = taskEvent.getLastModified();
    data.prescriptionId = taskEvent.getPrescriptionId().toString();
    data.hashedKvnr = model::HashedKvnr(taskEvent.getHashedKvnr());
    mEpaAccountLookup.lookupClient().addLogAttribute(data);
    auto acc = mEpaAccountLookup.lookup(taskEvent.getXRequestId(), taskEvent.getKvnr(),
                                        kvnr.useCachedValues() ? kvnr.getAssignedEpa() : std::nullopt);
    return acc;
}

void EventProcessor::scheduleRetryQueue(const model::EventKvnr& kvnr)
{
    auto retry = kvnr.getRetryCount();
    std::chrono::seconds retryDelaySeconds = calculateExponentialBackoffDelay(retry);
    autocommit([&](auto& db) {
        db.updateProcessingDelay(++retry, retryDelaySeconds, kvnr);
    });
}

void EventProcessor::scheduleHealthRecordRelocation(const model::EventKvnr& kvnr)
{
    autocommit([&](auto& db) {
        db.updateProcessingDelay(0, mHealthRecordRelocationWaitMinutes, kvnr);
    });
}

void EventProcessor::scheduleHealthRecordConflict(const model::EventKvnr& kvnr)
{
    autocommit([&](auto& db) {
        db.updateProcessingDelay(0, 24h, kvnr);
    });
}

std::chrono::seconds EventProcessor::calculateExponentialBackoffDelay(std::int32_t retry)
{
    return std::chrono::seconds(static_cast<std::int32_t>(std::pow(3, std::max(retry, 0))) * 60);
}

void EventProcessor::removeEuMedicationDispenseEvents(const model::EventKvnr &kvnr, std::vector<std::unique_ptr<model::TaskEvent>>& events)
{
    std::unordered_set<const model::TaskEvent*> eventsToRemove;
    for (const auto& event : events)
    {
        if (event->model::TaskEvent::getUseCase() != model::TaskEvent::UseCase::provideDispensation)
        {
            continue;
        }

        const auto& dispensationTaskEvent = dynamic_cast<const model::ProvideDispensationTaskEvent&>(*event);
        const auto& resources = dispensationTaskEvent.getMedicationDispenseBundle().getResourcesByType<model::MedicationDispense>();

        if (std::any_of(resources.begin(), resources.end(), [](const auto& r) {
            return r.getProfile() == model::ProfileType::GEM_ERPEU_PR_MedicationDispense;
        }))
        {
            eventsToRemove.insert(event.get());
        }
    }

    for (const auto* event : eventsToRemove)
    {
        EventProcessor::jsonLog() << kvnr << KeyValue("event", "Deleting event with eu medication dispenses")
                << KeyValue("prescription_id", event->model::TaskEvent::getPrescriptionId().model::PrescriptionId::toString());

        autocommit([&](auto& db) {
            db.deleteOneEventForKvnr(kvnr, event->model::TaskEvent::getId());
        });
    }

    events.erase(std::remove_if(events.begin(), events.end(),
                                [&eventsToRemove](const std::unique_ptr<model::TaskEvent>& event) {
                                    return eventsToRemove.contains(event.get());
                                }),
                 events.end());
}


boost::asio::awaitable<void>
EventProcessor::runloopWorker(RunLoopScheduler& scheduler,
                              std::weak_ptr<MedicationExporterServiceContext> serviceContext)
{
    using namespace std::chrono_literals;

    boost::asio::steady_timer timer{scheduler.getThreadPool().ioContext()};
    while (! scheduler.getThreadPool().ioContext().stopped())
    {
        RunLoopScheduler::Reschedule rescheduleRule{RunLoopScheduler::Reschedule::Delayed};
        try
        {
            auto serviceCtx = serviceContext.lock();
            if (! serviceCtx)
            {
                co_return;
            }
            if (scheduler.checkIsPaused(serviceCtx, exporter::RuntimeConfiguration::ProcessorType::EPA))
            {
                serviceCtx.reset();
                timer.expires_after(100ms);
                co_await timer.async_wait(boost::asio::as_tuple(boost::asio::deferred));
                continue;
            }

            EpaAccountLookup accountLookup(*serviceCtx);
            if (EventProcessor{serviceCtx, accountLookup}.process())
            {
                if (scheduler.checkIsThrottled(serviceCtx))
                {
                    rescheduleRule = RunLoopScheduler::Reschedule::Throttled;
                }
                else
                {
                    rescheduleRule = RunLoopScheduler::Reschedule::Immediate;
                }
            }
            else
            {
                rescheduleRule = RunLoopScheduler::Reschedule::Delayed;
            }
        }
        catch (const std::exception& e)
        {
            rescheduleRule = RunLoopScheduler::Reschedule::TemporaryError;
            std::string reason = dynamic_cast<const model::ModelException*>(&e) ? "not given" : e.what();
            JsonLog(LogId::INFO, JsonLog::makeInfoLogReceiver(), false)
                .keyValue("event", "Processing task events: Unexpected error. Wait before trying next processing.")
                .keyValue("reason", reason)
                .locationFromException(e);
        }
        switch (rescheduleRule)
        {
            case RunLoopScheduler::Reschedule::Immediate:
                // return into executor to allow handling of other events (like SignalHandler)
                co_await async_immediate(co_await boost::asio::this_coro::executor);
                break;
            case RunLoopScheduler::Reschedule::Delayed:
                timer.expires_after(std::chrono::seconds(5));
                co_await timer.async_wait(boost::asio::as_tuple(boost::asio::deferred));
                break;
            case RunLoopScheduler::Reschedule::Throttled:
                timer.expires_after(scheduler.getThrottleValue());
                co_await timer.async_wait(boost::asio::as_tuple(boost::asio::deferred));
                break;
            case RunLoopScheduler::Reschedule::TemporaryError:
                // handle KVNR processing error
                timer.expires_after(std::chrono::seconds(60));
                co_await timer.async_wait(boost::asio::as_tuple(boost::asio::deferred));
                break;
        }
    }
}
