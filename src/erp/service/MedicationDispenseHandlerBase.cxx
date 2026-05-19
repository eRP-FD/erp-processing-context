/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/MedicationDispenseHandlerBase.hxx"
#include "erp/model/MedicationsAndDispenses.hxx"
#include "erp/model/eu/GemErpEuPrMedication.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/model/Bundle.hxx"
#include "shared/model/GemErpPrMedication.hxx"
#include "shared/model/KbvMedicationBase.hxx"
#include "shared/model/MedicationDispense.hxx"
#include "shared/model/MedicationDispenseBundle.hxx"
#include "shared/model/MedicationDispenseId.hxx"
#include "shared/model/MedicationDispenseOperationParameters.hxx"
#include "shared/util/Demangle.hxx"
#include "shared/util/Uuid.hxx"

MedicationDispenseHandlerBase::MedicationDispenseHandlerBase(
    Operation operation, const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ErpRequestHandler{operation, allowedProfessionOiDs}
{
}

model::MedicationsAndDispenses MedicationDispenseHandlerBase::parseBody(PcSessionContext& session,
                                                                        Operation forOperation,
                                                                        model::PrescriptionType workflow)
{
    try
    {
        A_22069_02.start("Task schließen - Speicherung mehrerer MedicationDispense");
        A_24283_03.start("Dispensierinformationen bereitstellen - Speicherung mehrerer MedicationDispenses");
        auto params = parseAndValidateRequestBody<model::MedicationDispenseOperationParameters>(
            session, {parameterTypeFor(forOperation)});
        if (const auto profileVersion = params.getProfileVersionChecked())
        {
            session.addOuterResponseHeaderField(Header::GematikWorkflowProfil, to_string(*profileVersion));
        }
        A_24283_03.finish();
        A_22069_02.finish();
        return medicationDispensesFromParameters(params, workflow);
    }
    catch (const model::ModelException& e)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Error during request parsing.", e.what());
    }
    Fail2("MedicationDispenseHandlerBase::parseBody failed", std::logic_error);
}

void MedicationDispenseHandlerBase::checkSingleOrBundleAllowed(const model::MedicationDispense& medicationDispense)
{
    auto profileName = medicationDispense.getProfileName();
    ModelExpect(profileName.has_value(), "Missing meta.profile in MedicationDispense");
    fhirtools::DefinitionKey profileKey{*profileName};
    ModelExpect(profileKey.version.has_value(), "No version in meta.profile of MedicationDispense");
    ModelExpect(value(profileKey.version) < model::version::GEM_ERP_1_4,
                "MedicationDispense in versions 1.4 or later must be provided as Parameters");
}

model::ProfileType MedicationDispenseHandlerBase::parameterTypeFor(Operation operation)
{

    switch (operation)
    {
        case Operation::POST_Task_id_dispense:
            return model::ProfileType::GEM_ERP_PR_PAR_DispenseOperation_Input;
        case Operation::POST_Task_id_close:
            return model::ProfileType::GEM_ERP_PR_PAR_CloseOperation_Input;
        case Operation::POST_Task_id_eu_close:
            return model::ProfileType::GEM_ERPEU_PR_PAR_CloseOperation_Input;
        case Operation::UNKNOWN:
        case Operation::GET_Task:
        case Operation::GET_Task_id:
        case Operation::POST_Task_create:
        case Operation::POST_Task_id_activate:
        case Operation::POST_Task_id_accept:
        case Operation::POST_Task_id_reject:
        case Operation::POST_Task_id_abort:
        case Operation::PATCH_Task_id_mark:
        case Operation::GET_MedicationDispense:
        case Operation::GET_MedicationDispense_id:
        case Operation::GET_Communication:
        case Operation::GET_Communication_id:
        case Operation::POST_Communication:
        case Operation::DELETE_Communication_id:
        case Operation::GET_AuditEvent:
        case Operation::GET_AuditEvent_id:
        case Operation::GET_Device:
        case Operation::GET_metadata:
        case Operation::DELETE_ChargeItem_id:
        case Operation::GET_ChargeItem:
        case Operation::GET_ChargeItem_id:
        case Operation::POST_ChargeItem:
        case Operation::PUT_ChargeItem_id:
        case Operation::DELETE_Consent:
        case Operation::PATCH_ChargeItem_id:
        case Operation::GET_Consent:
        case Operation::POST_Consent:
        case Operation::GET_notifications_opt_in:
        case Operation::GET_notifications_opt_out:
        case Operation::POST_VAU_up:
        case Operation::GET_Health:
        case Operation::POST_Subscription:
        case Operation::POST_Admin_restart:
        case Operation::GET_Admin_configuration:
        case Operation::PUT_Admin_pn3_configuration:
        case Operation::PUT_Admin_exporter_configuration:
        case Operation::GET_Admin_metrics:
        case Operation::POST_grant_eu_access_permission:
        case Operation::DELETE_revoke_eu_access_permission:
        case Operation::GET_read_eu_access_permission:
        case Operation::GET_eu_prescriptions:
            Fail2("MedicationDispenseHandlerBase: invalid operation:" + std::string{magic_enum::enum_name(operation)},
                  std::logic_error);
    }
    Fail2("MedicationDispenseHandlerBase: unknown operation:" + std::to_string(static_cast<uintmax_t>(operation)),
          std::logic_error);
}

model::MedicationsAndDispenses MedicationDispenseHandlerBase::medicationDispensesFromParameters(
    const model::MedicationDispenseOperationParameters& parameters, model::PrescriptionType workflow)
{
    model::MedicationsAndDispenses result;
    if (isDiga(workflow))
    {
        result.addFromDigaParameters(parameters);
    }
    else
    {
        result.addFromParameters(parameters);
    }
    return result;
}

void MedicationDispenseHandlerBase::checkMedicationDispenses(
    std::vector<model::MedicationDispense>& medicationDispenses, const model::PrescriptionId& prescriptionId,
    const model::Kvnr& kvnr, const std::string& telematikIdFromAccessToken)
{
    for (size_t i = 0, end = medicationDispenses.size(); i < end; ++i)
    {
        auto& medicationDispense = medicationDispenses[i];
        A_19248_05.start("Check provided MedicationDispense object, especially PrescriptionID, KVNR and TelematikID");

        // See https://simplifier.net/erezept-workflow/GemerxMedicationDispense/~details
        // The logical id of the resource, as used in the URL for the resource. Once assigned, this value never changes.
        // The only time that a resource does not have an id is when it is being submitted to the server using a create operation.
        // As medication dispenses are queried by "GET /MedicationDispense/{Id}" where Id equals the TaskId (= prescriptionId)
        // the Id of the medication dispense resource is set to the prescriptionId.
        // Check for correct MedicationDispense.identifier
        try
        {
            ErpExpect(medicationDispense.prescriptionId() == prescriptionId, HttpStatus::BadRequest,
                      "Prescription ID in MedicationDispense does not match the one in the task");
            // test if whenPrepared and whenHandedOver can be converted to Timestamp:
            (void) medicationDispense.whenPrepared();
            (void) medicationDispense.whenHandedOver();
            ErpExpect(medicationDispense.kvnr() == kvnr, HttpStatus::BadRequest,
                      "KVNR in MedicationDispense does not match the one in the task");

            if (medicationDispense.profileType() != model::ProfileType::GEM_ERPEU_PR_MedicationDispense)
            {
                ErpExpect(medicationDispense.telematikId() == telematikIdFromAccessToken, HttpStatus::BadRequest,
                          "Telematik-ID in MedicationDispense does not match the one in the access token.");
            }
        }
        catch (const model::ModelException& exc)
        {
            TVLOG(1) << "ModelException: " << exc.what();
            ErpFailWithDiagnostics(HttpStatus::BadRequest, "Invalid MedicationDispense", exc.what());
        }
        medicationDispense.setId({prescriptionId, i});

        A_19248_05.finish();
    }
}


model::MedicationDispenseBundle
MedicationDispenseHandlerBase::createBundle(const model::MedicationsAndDispenses& bodyData)
{
    return model::MedicationDispenseBundle(getLinkBase(), bodyData.medicationDispenses, bodyData.medications);
}
