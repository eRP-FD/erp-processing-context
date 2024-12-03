#include "EventProcessor.hxx"
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
    , mConflictDelaySeconds(std::chrono::duration_cast<std::chrono::seconds>(24h))
{
}

bool EventProcessor::process()
{
    // Workflow step 1
    const auto kvnr = createMedicationExporterCommitGuard().db().processNextKvnr();

    if (kvnr)
    {
        processOne(*kvnr);
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
        writeAuditEvent(auditDataCollector);
        A_25962.finish();

        // Workflow step 11 - check ePA response
        switch (outcome)
        {
            case eventprocessing::Outcome::Success: {
                // Workflow step 12 - delete successfully transmitted event
                jsonLog()
                    .keyValue("event", "Deleting event after successful transmission")
                    .keyValue("prescription_id", event->getPrescriptionId().toString());
                createMedicationExporterCommitGuard().db().deleteOneEventForKvnr(kvnr, event->getId());
            }
            break;
            case eventprocessing::Outcome::Retry:
                // Workflow step 15 - set kvnr to pending and reschedule it
                {
                    jsonLog()
                        .keyValue("event", "Technical error occurred. Rescheduling KVNR")
                        .keyValue("kvnr", kvnr.getLoggingId());
                    createMedicationExporterCommitGuard().db().postponeProcessing(mRetryDelaySeconds, kvnr);
                }
                break;
            case eventprocessing::Outcome::DeadLetter:
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
    createMedicationExporterCommitGuard().db().postponeProcessing(mConflictDelaySeconds, kvnr);
    jsonLog()
        .keyValue("event", "Information service responded with conflict (Aktenumzug) for KVNR. Reschedule KVNR in 24h.")
        .keyValue("kvnr", kvnr.getLoggingId());
}

void EventProcessor::processEpaUnknown(const model::EventKvnr& kvnr)
{
    jsonLog().keyValue("event", "Technical error occurred. Rescheduling KVNR").keyValue("kvnr", kvnr.getLoggingId());
    createMedicationExporterCommitGuard().db().postponeProcessing(mRetryDelaySeconds, kvnr);
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
    const auto& config = Configuration::instance();
    auto enforceServerAuth =
        config.getBoolValue(ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_ENFORCE_SERVER_AUTHENTICATION);
    TlsCertificateVerifier verifier =
        enforceServerAuth
            ? TlsCertificateVerifier::withTslVerification(
                  mServiceContext->getTslManager(), {.certificateType = CertificateType::C_FD_TLS_S,
                                                     .ocspGracePeriod = std::chrono::seconds(config.getIntValue(
                                                         ConfigurationKey::MEDICATION_EXPORTER_OCSP_EPA_GRACE_PERIOD)),
                                                     .withSubjectAlternativeAddressValidation = true})
                  .withAdditionalCertificateCheck([](const X509Certificate& cert) -> bool {
                      return cert.checkRoles({std::string{profession_oid::oid_epa_dvw}});
                  })
            : TlsCertificateVerifier::withVerificationDisabledForTesting();
    return EpaAccountLookup(verifier).addLogAttribute("prescriptionId", taskEvent.getPrescriptionId().toString()).lookup(taskEvent.getKvnr());
}

MedicationExporterCommitGuard EventProcessor::createMedicationExporterCommitGuard()
{
    return MedicationExporterCommitGuard(mServiceContext->medicationExporterDatabaseFactory());
}
