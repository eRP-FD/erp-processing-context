#include "EventProcessor.hxx"
#include "BdeMessage.hxx"
#include "database/CommitGuard.hxx"
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
#include "shared/util/Configuration.hxx"
#include "shared/util/Demangle.hxx"
#include "shared/util/JsonLog.hxx"

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
        const auto events = autocommit([&](auto& db) {
            return db.getAllEventsForKvnr(kvnr);
        });

        jsonLog() << kvnr << KeyValue("retryCount", std::to_string(kvnr.getRetryCount()))
                  << KeyValue("event", "Start Processing KVNR with " + std::to_string(events.size()) + " events");
        if (! events.empty())
        {
            const auto& firstEvent = *events.front();
            ScopedLogContext scopedLogContext{firstEvent.getXRequestId()};

            auto durationConsumerGuard{std::make_unique<DurationConsumerGuard>(firstEvent.getXRequestId())};
            // Workflow step 3 - decodedKvnr = decrypt(events.front()).kvnr
            auto epaAccount = ePaAccountLookup(firstEvent);
            durationConsumerGuard.reset();

            // Workflow step 4 - ePA account lookup
            switch (epaAccount.lookupResult)
            {
                case EpaAccount::Code::allowed:
                    // Workflow step 4a
                        TVLOG(1) << "account lookup allowed with given kvnr";
                processEpaAllowed(kvnr, epaAccount, events);
                break;
                case EpaAccount::Code::deny:
                    [[fallthrough]];
                case EpaAccount::Code::notFound:
                    // Workflow step 4b - deny / not found -> Workflow step 14
                        processEpaDeniedOrNotFound(kvnr);
                break;
                case EpaAccount::Code::conflict:
                    // Workflow step 4c
                        processEpaConflict(kvnr);
                break;
                case EpaAccount::Code::unknown:
                    // Workflow step 4d
                        TVLOG(1) << "account lookup unknown with given kvnr";
                processEpaUnknown(kvnr);
                break;
            }
        }
        else
        {
            // Workflow step 13 - set kvnr to processed
            autocommit([&](auto& db) {
                db.finalizeKvnr(kvnr);
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
        DurationConsumerGuard durationConsumerGuard{event->getXRequestId()};
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
    // Workflow step 13 - set kvnr to procesesd
    autocommit([&](auto& db) {
        db.finalizeKvnr(kvnr);
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

    // Workflow step 13 - set kvnr to procesesd
    autocommit([&](auto& db) {
        db.finalizeKvnr(kvnr);
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

EpaAccount EventProcessor::ePaAccountLookup(const model::TaskEvent& taskEvent)
{
    mEpaAccountLookup.lookupClient()
        .addLogAttribute(BDEMessage::lastModifiedTimestampKey, taskEvent.getLastModified())
        .addLogAttribute(BDEMessage::prescriptionIdKey, taskEvent.getPrescriptionId().toString())
        .addLogAttribute(BDEMessage::hashedKvnrKey,  std::make_optional<model::HashedKvnr>(taskEvent.getHashedKvnr()));
    return mEpaAccountLookup.lookup(taskEvent.getXRequestId(), taskEvent.getKvnr());
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
