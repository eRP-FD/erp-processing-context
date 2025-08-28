/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/task/eu/EuCloseTaskHandler.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/MedicationsAndDispenses.hxx"
#include "erp/model/eu/EuAccessCode.hxx"
#include "erp/model/eu/EuAccessPermission.hxx"
#include "erp/model/eu/GemErpEuPrParCloseOperationInput.hxx"
#include "shared/crypto/SignedPrescription.hxx"
#include "shared/model/MedicationDispenseBundle.hxx"
#include "shared/model/MedicationDispenseId.hxx"
#include "shared/util/Base64.hxx"

namespace eu
{

namespace
{
void updateReferencesAndIds(model::MedicationsAndDispenses& medicationsAndDispenses,
                            model::GemErpEuPrPractitioner& practitioner,
                            model::GemErpEuPrPractitionerRole& practitionerRole,
                            model::GemErpEuPrOrganization& organization)
{
    const Uuid practitionerId{};
    practitioner.setId(practitionerId.toString());

    const Uuid organizationId{};
    organization.setId(organizationId.toString());

    const Uuid practitionerRoleId{};
    practitionerRole.setId(practitionerRoleId.toString());
    practitionerRole.setPractitionerReference(practitionerId.toUrn());
    practitionerRole.setOrganizationReference(organizationId.toUrn());

    for (auto& md : medicationsAndDispenses.medicationDispenses)
    {
        // All medication dispensations refer to the same practitioner.
        md.setPerformerReference(practitionerRoleId.toUrn());
    }
}
}

EuCloseTaskHandler::EuCloseTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : MedicationDispenseHandlerBase(Operation::POST_Task_id_eu_close, allowedProfessionOiDs)
{
}

void EuCloseTaskHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto prescriptionId = parseId(session.request, session.accessLog);

    TVLOG(1) << "Working on Task for prescription id " << prescriptionId.toString();

    try
    {
        A_27069.start("FHIR validation with GEM_ERPEU_PR_PAR_CloseOperation_Input profile.");
        const auto validatedDoc = parseAndValidateRequestBody<model::GemErpEuPrParCloseOperationInput>(session);

        ErpExpect(!session.request.getBody().empty(), HttpStatus::BadRequest, "Missing request body.");
        model::MedicationsAndDispenses medicationsAndDispenses = parseBody(session, Operation::POST_Task_id_eu_close, prescriptionId.type());

        A_27069.finish();

        const auto kvnr = validatedDoc.kvnr();
        const auto accessCode = validatedDoc.accessCode();
        const auto countryCode = validatedDoc.countryCode();
        const auto practitionerName = validatedDoc.practitionerName();
        const auto healthcareFacilityType = validatedDoc.healthcareFacilityType();
        const auto pointOfCare = validatedDoc.pointOfCare();

        auto* databaseHandle = session.database();

        // Get task and check for 'in-progress' status
        auto [taskAndKey, prescription] = databaseHandle->retrieveTaskForUpdateAndPrescription(prescriptionId);
        ErpExpect(taskAndKey.has_value(), HttpStatus::NotFound, "Task not found for prescription id");
        std::optional<model::Task> task = std::move(taskAndKey->task);

        ErpExpect(task->type() == model::PrescriptionType::apothekenpflichigeArzneimittel ||
            task->type() == model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
            HttpStatus::BadRequest,
            "Only apothekenpflichigeArzneimittel|Pkv prescriptions allowed.");

        const auto taskStatus = task->status();
        A_27072.start("Task Statusprüfung");
        ErpExpect(taskStatus == model::Task::Status::inprogress, HttpStatus::Forbidden,
                  "Task has to be in progress, but is: " + std::string(model::Task::StatusNames.at(taskStatus)));
        A_27072.finish();

        // Check for kvnr eu-dispensation-consent
        A_27070.start("Prüfung Einwilligung für KVNR");
        const auto consent = databaseHandle->retrieveConsent(kvnr, model::ConsentType::EUDISPCONS);
        ErpExpect(consent, HttpStatus::Forbidden, "Patienteneinwilligung nicht vorhanden");
        A_27070.finish();

        A_27071.start("Prüfung Zugriffsberechtigung");
        std::optional<model::EuAccessPermission> storedAccessPermission =
            databaseHandle->retrieveEuAccessPermission(kvnr);
        ErpExpect(storedAccessPermission.has_value() && storedAccessPermission->isValid(model::Timestamp::now()),
                  HttpStatus::Forbidden, "Access permission not found");
        ErpExpect(storedAccessPermission->getAccessCode() == accessCode &&
                      storedAccessPermission->getCountryCode() == countryCode,
                  HttpStatus::Forbidden, "Access permission not found");
        A_27071.finish();

        Expect3(task->kvnr().has_value(), "Task has no KV number", std::logic_error);
        Expect3(task->kvnr().value().id() == kvnr.id(), "Invalid KVNR", std::logic_error);

        checkMedicationDispenses(medicationsAndDispenses.medicationDispenses, prescriptionId, task->kvnr().value(), {});

        A_27074.start("MedicationDispense Zeitstempel aktualisieren");
        task->updateLastMedicationDispense();
        A_27074.finish();

        A_27075.start("Task Status aktualisieren");
        task->setStatus(model::Task::Status::completed);
        A_27075.finish();

        auto practitioner = validatedDoc.practitionerModel();
        auto practitionerRole = validatedDoc.practitionerRoleModel();
        auto organization = validatedDoc.organizationModel();
        updateReferencesAndIds(medicationsAndDispenses, practitioner, practitionerRole, organization);

        databaseHandle->updateTaskMedicationDispense(
            *task, createEuBundle(medicationsAndDispenses, practitioner, practitionerRole, organization));

        makeResponse(session, HttpStatus::OK, nullptr);

        // Collect Audit data
        session.auditDataCollector()
            .setVariable("practitioner",
                         std::string{validatedDoc.practitionerRole()} + " " + std::string{practitionerName})
            .setVariable("facility", std::string{healthcareFacilityType} + " " + std::string{pointOfCare})
            .setCountryCode(countryCode)
            .setEventId(model::AuditEventId::POST_TASK_EU_CLOSE)
            .setInsurantKvnr(kvnr)
            .setAction(model::AuditEvent::Action::update)
            .setPrescriptionId(prescriptionId);
    }
    catch (const model::ModelException& e)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "parsing/validation error", e.what());
    }
}

model::MedicationDispenseBundle EuCloseTaskHandler::createEuBundle(
    const model::MedicationsAndDispenses& medicationsAndDispenses, const model::GemErpEuPrPractitioner& practitioner,
    const model::GemErpEuPrPractitionerRole& practitionerRole, const model::GemErpEuPrOrganization& organization)
{
    auto bundle = MedicationDispenseHandlerBase::createBundle(medicationsAndDispenses);
    bundle.addEuResource(std::string{value(practitioner.getId())}, practitioner.jsonDocument());
    bundle.addEuResource(std::string{value(practitionerRole.getId())}, practitionerRole.jsonDocument());
    bundle.addEuResource(std::string{value(organization.getId())}, organization.jsonDocument());
    return bundle;
}

}
