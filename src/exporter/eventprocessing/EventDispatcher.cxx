/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/eventprocessing/EventDispatcher.hxx"
#include "exporter/client/EpaMedicationClient.hxx"
#include "exporter/eventprocessing/CancelPrescription.hxx"
#include "exporter/eventprocessing/ProvideDispensation.hxx"
#include "exporter/eventprocessing/ProvidePrescription.hxx"
#include "exporter/model/DeviceId.hxx"
#include "exporter/model/TaskEvent.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/audit/AuditDataCollector.hxx"

namespace eventprocessing
{

EventDispatcher::EventDispatcher(std::unique_ptr<IEpaMedicationClient>&& medicationClient)
    : mMedicationClient(std::move(medicationClient))
{
}

Outcome EventDispatcher::dispatch(const model::TaskEvent& erpEvent, AuditDataCollector& auditDataCollector)
{
    A_19296_03.start("AuditEvent Content");
    auditDataCollector.setAgentName("E-Rezept-Fachdienst");
    auditDataCollector.setAgentType(model::AuditEvent::AgentType::machine);
    auditDataCollector.setAgentWho("9-E-Rezept-Fachdienst");
    auditDataCollector.setDeviceId(ExporterDeviceId);
    auditDataCollector.setPrescriptionId(erpEvent.getPrescriptionId());
    auditDataCollector.setInsurantKvnr(erpEvent.getKvnr());

    mMedicationClient->addLogData("prescriptionId", erpEvent.getPrescriptionId().toString());
    mMedicationClient->addLogData("lastModifiedTimestamp", erpEvent.getLastModified());

    std::optional<model::AuditEventId> successAuditType;
    std::optional<model::AuditEventId> failedAuditType;
    Outcome outcome = Outcome::DeadLetter;
    try
    {
        switch (erpEvent.getUseCase())
        {
            // Workflow step 8 - ePa call providePrescription
            case model::TaskEvent::UseCase::providePrescription:
                A_25962.start("Die Verordnung wurde in die Patientenakte übertragen. / Die Verordnung konnte nicht in die Patientenakte übertragen werden.");
                auditDataCollector.setAction(model::AuditEvent::Action::create);
                successAuditType = model::AuditEventId::POST_PROVIDE_PRESCRIPTION_ERP;
                failedAuditType = model::AuditEventId::POST_PROVIDE_PRESCRIPTION_ERP_failed;
                A_25962.finish();
                outcome = ProvidePrescription::process(mMedicationClient.get(), dynamic_cast<const model::ProvidePrescriptionTaskEvent&>(erpEvent));
                break;
            // Workflow step 9 - ePa call cancelPrescription
            case model::TaskEvent::UseCase::cancelPrescription:
                A_25962.start("Die Löschinformation zum E-Rezept wurde in die Patientenakte übermittelt. / Die Löschinformation zum E-Rezept konnte nicht in die Patientenakte übermittelt werden.");
                auditDataCollector.setAction(model::AuditEvent::Action::del);
                successAuditType = model::AuditEventId::POST_CANCEL_PRESCRIPTION_ERP;
                failedAuditType = model::AuditEventId::POST_CANCEL_PRESCRIPTION_ERP_failed;
                A_25962.finish();
                outcome = CancelPrescription::process(mMedicationClient.get(), dynamic_cast<const model::CancelPrescriptionTaskEvent&>(erpEvent));
                break;
            // Workflow step 10 - ePa call ProvideDispensation
            case model::TaskEvent::UseCase::provideDispensation:
                A_25962.start("Die Medikamentenabgabe wurde in die Patientenakte übertragen. / Die Medikamentenabgabe konnte nicht in die Patientenakte übertragen werden.");
                auditDataCollector.setAction(model::AuditEvent::Action::create);
                successAuditType = model::AuditEventId::POST_PROVIDE_DISPENSATION_ERP;
                failedAuditType = model::AuditEventId::POST_PROVIDE_DISPENSATION_ERP_failed;
                A_25962.finish();
                outcome = ProvideDispensation::process(mMedicationClient.get(), dynamic_cast<const model::ProvideDispensationTaskEvent&>(erpEvent));
                break;
            case model::TaskEvent::UseCase::cancelDispensation:
                A_25962.start("Die Löschinformation für die Medikamentenabgabe wurde in die Patientenakte übertragen. / Die Löschinformation für die Medikamentenabgabe konnte nicht in die Patientenakte übertragen werden.");
                auditDataCollector.setAction(model::AuditEvent::Action::del);
                successAuditType = model::AuditEventId::POST_CANCEL_DISPENSATION_REP;
                failedAuditType = model::AuditEventId::POST_CANCEL_DISPENSATION_REP_failed;
                A_25962.finish();
                break;
        }
    }
    catch (const std::runtime_error& re)
    {
        outcome = Outcome::DeadLetter;
    }
    Expect(successAuditType.has_value() && failedAuditType.has_value(),
           "implementation error: successAuditType and/or failedAuditType not set");
    switch (outcome)
    {
        case Outcome::Success:
            auditDataCollector.setEventId(*successAuditType);
            break;
        case Outcome::Retry:
            // no audit event
            break;
        case Outcome::DeadLetter:
            auditDataCollector.setEventId(*failedAuditType);
            break;
    }
    A_19296_03.finish();

    return outcome;
}

}
