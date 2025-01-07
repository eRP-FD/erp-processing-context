#include "EventProcessor.hxx"
#include "BdeMessage.hxx"
#include "database/CommitGuard.hxx"
#include "exporter/client/EpaMedicationClientImpl.hxx"
#include "exporter/database/MedicationExporterDatabaseFrontend.hxx"
#include "exporter/eventprocessing/EventDispatcher.hxx"
#include "exporter/model/EventKvnr.hxx"
#include "exporter/model/TaskEvent.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/audit/AuditDataCollector.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Demangle.hxx"
#include "shared/util/JsonLog.hxx"

using namespace std::chrono_literals;

EventProcessor::EventProcessor(const std::shared_ptr<MedicationExporterServiceContext>& serviceContext)
    : jsonLog([] {
        return JsonLog(LogId::INFO, JsonLog::makeInfoLogReceiver(), false);
    })
    , mServiceContext(serviceContext)
    , mRetryDelaySeconds(Configuration::instance().getIntValue(
          ConfigurationKey::MEDICATION_EXPORTER_EPA_PROCESSING_OUTCOME_RETRY_DELAY_SECONDS))
    , mHealthRecordRelocationWaitMinutes(Configuration::instance().getOptionalIntValue(
          ConfigurationKey::MEDICATION_EXPORTER_EPA_CONFLICT_WAIT_MINUTES, 1440))
{
}

bool EventProcessor::process()
{
    // Workflow step 1
    const auto kvnr = createMedicationExporterCommitGuard().db().processNextKvnr();

    if (kvnr)
    {
        try
        {
            processOne(*kvnr);
        }
        catch (const std::exception& e)
        {
            std::string reason = dynamic_cast<const model::ModelException*>(&e) ? "not given" : e.what();
            jsonLog()
                .keyValue("event", "Processing task events: Unexpected error. Rescheduling KVNR")
                .keyValue("kvnr", kvnr->getLoggingId())
                .keyValue("reason", reason)
                .locationFromException(e);
            scheduleRetryQueue(*kvnr);
        }
        return true;
    }
    // Nothing processed.
    return false;
}

void EventProcessor::processOne(const model::EventKvnr& kvnr)
{
    // Workflow step 2
    const auto events = createMedicationExporterCommitGuard().db().getAllEventsForKvnr(kvnr);

    jsonLog()
        .keyValue("event", "Start Processing KVNR with " + std::to_string(events.size()) + " events")
        .keyValue("kvnr", kvnr.getLoggingId());
    if (! events.empty())
    {
        const auto& firstEvent = *events.front();

        // Workflow step 3 - decodedKvnr = decrypt(events.front()).kvnr
        auto epaAccount = ePaAccountLookup(firstEvent);

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
        createMedicationExporterCommitGuard().db().finalizeKvnr(kvnr);
        TLOG(WARNING) << "KVNR to process had no events";
    }
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
        Expect(event, "No or bad events for kvnr");

        // Workflow step 6 - check deadletter
        bool deadLetter = createMedicationExporterCommitGuard().db().isDeadLetter(kvnr, event->getPrescriptionId(),
                                                                                  event->getPrescriptionType());
        // Workflow step 6b - if event in deadletterQueue -> Workflow step 17
        if (deadLetter)
        {
            // Workflow step 17 - set deadletter
            const auto skipped = createMedicationExporterCommitGuard().db().markDeadLetter(
                kvnr, event->getPrescriptionId(), event->getPrescriptionType());
            TVLOG(1) << "marked events as deadletterQueue: " << skipped;
            jsonLog()
                .keyValue("event", "Setting all events for task in dead letter queue")
                .keyValue("prescription_id", event->getPrescriptionId().toString());
            continue;
        }

        // Workflow step 7 - check usecase of event (and execute step 8,9 or 10)
        jsonLog()
            .keyValue("event", "Processing event")
            .keyValue("prescription_id", event->getPrescriptionId().toString())
            .keyValue("usecase", magic_enum::enum_name(event->getUseCase()))
            .keyValue("epa", epaAccount.host);
        auto outcome = dispatcher.dispatch(*event, auditDataCollector);

        A_25962.start("Write audit event to database");
        if (outcome != eventprocessing::Outcome::Retry)
        {
            writeAuditEvent(auditDataCollector);
        }
        A_25962.finish();

        // Workflow step 11 - check ePA response
        switch (outcome)
        {
            case eventprocessing::Outcome::Success: {// Workflow step 11a - Success -> Workflow step 15
                // Workflow step 12 - delete successfully transmitted event
                jsonLog()
                    .keyValue("event", "Deleting event after successful transmission")
                    .keyValue("prescription_id", event->getPrescriptionId().toString());
                createMedicationExporterCommitGuard().db().deleteOneEventForKvnr(kvnr, event->getId());
            }
            break;
            case eventprocessing::Outcome::Retry:// Workflow step 11c - RetryQueue -> Workflow step 15
                // Workflow step 15 - set kvnr to pending and reschedule it
                {
                    jsonLog()
                        .keyValue("event", "Technical error occurred. Rescheduling KVNR")
                        .keyValue("kvnr", kvnr.getLoggingId());
                    scheduleRetryQueue(kvnr);
                    return;// break transmission of remaining events of this KVNR
                }
                break;
            case eventprocessing::Outcome::DeadLetter:// Workflow step 11b - DeadletterQueue -> Workflow step 17
                // Workflow step 17 - set deadletter
                const auto skipped = createMedicationExporterCommitGuard().db().markDeadLetter(
                    kvnr, event->getPrescriptionId(), event->getPrescriptionType());
                TVLOG(1) << "marked events as deadletterQueue: " << skipped;
                jsonLog()
                    .keyValue("event", "Setting all events for task in dead letter queue")
                    .keyValue("prescription_id", event->getPrescriptionId().toString());
                break;
        }
    }
    // Workflow step 13 - set kvnr to procesesd
    createMedicationExporterCommitGuard().db().finalizeKvnr(kvnr);
    jsonLog().keyValue("event", "KVNR processed").keyValue("kvnr", kvnr.getLoggingId());
}

void EventProcessor::processEpaDeniedOrNotFound(const model::EventKvnr& kvnr)
{
    TVLOG(1) << "account lookup denied / not found with given kvnr";

    // Workflow step 14 - delete kvnr events
    createMedicationExporterCommitGuard().db().deleteAllEventsForKvnr(kvnr);

    jsonLog().keyValue("event", "Delete all Task Events for KVNR").keyValue("kvnr", kvnr.getLoggingId());

    // Workflow step 13 - set kvnr to procesesd
    createMedicationExporterCommitGuard().db().finalizeKvnr(kvnr);
}

void EventProcessor::processEpaConflict(const model::EventKvnr& kvnr)
{
    TVLOG(1) << "account lookup conflict with given kvnr";
    scheduleHealthRecordRelocation(kvnr);
    jsonLog()
        .keyValue("event", "Information service responded with conflict (Aktenumzug) for KVNR. Reschedule KVNR in 24h.")
        .keyValue("kvnr", kvnr.getLoggingId());
}

void EventProcessor::processEpaUnknown(const model::EventKvnr& kvnr)
{
    jsonLog().keyValue("event", "Technical error occurred. Rescheduling KVNR").keyValue("kvnr", kvnr.getLoggingId());
    scheduleRetryQueue(kvnr);
}

void EventProcessor::writeAuditEvent(const AuditDataCollector& auditDataCollector)
{
    try
    {
        model::AuditData auditData = auditDataCollector.createData();
        MainDatabaseCommitGuard(mServiceContext->erpDatabaseFactory()).db().storeAuditEventData(auditData);
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
    auto accountLookup = EpaAccountLookup(*mServiceContext);
    accountLookup.lookupClient().addLogAttribute(BDEMessage::lastModifiedTimestampKey, taskEvent.getLastModified()).addLogAttribute(BDEMessage::prescriptionIdKey, taskEvent.getPrescriptionId().toString());
    return accountLookup.lookup(taskEvent.getKvnr());
}

void EventProcessor::scheduleRetryQueue(const model::EventKvnr& kvnr)
{
    auto retry = kvnr.getRetryCount();
    std::chrono::seconds retryDelaySeconds = calculateExponentialBackoffDelay(retry);
    createMedicationExporterCommitGuard().db().updateProcessingDelay(++retry, retryDelaySeconds, kvnr);
}

void EventProcessor::scheduleHealthRecordRelocation(const model::EventKvnr& kvnr)
{
    createMedicationExporterCommitGuard().db().updateProcessingDelay(0, mHealthRecordRelocationWaitMinutes, kvnr);
}

std::chrono::seconds EventProcessor::calculateExponentialBackoffDelay(std::int32_t retry)
{
    return std::chrono::seconds(static_cast<std::int32_t>(std::pow(3, std::max(retry, 0))) * 60);
}

MedicationExporterCommitGuard EventProcessor::createMedicationExporterCommitGuard()
{
    return MedicationExporterCommitGuard(mServiceContext->medicationExporterDatabaseFactory());
}
