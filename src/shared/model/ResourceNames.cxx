#include "ResourceNames.hxx"
#include "shared/util/Expect.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"

#include <map>

std::optional<std::string_view> model::profile(ProfileType profileType)
{
    switch (profileType)
    {
        using enum ProfileType;
        namespace sd = resource::structure_definition;
        case fhir:// general FHIR
            return std::nullopt;
        case ActivateTaskParameters:
        case CreateTaskParameters:
        case PatchChargeItemParameters:
            return sd::parameters;
        case ProvidePrescriptionErpOp:
            return sd::epa_op_provide_prescription_erp_input_parameters;
        case ProvideDispensationErpOp:
            return sd::epa_op_provide_dispensation_erp_input_parameters;
        case CancelPrescriptionErpOp:
            return sd::epa_op_cancel_prescription_erp_input_parameters;
        case EPAOpRxPrescriptionERPOutputParameters:
            return sd::epa_op_rx_prescription_erp_output_parameters;
        case EPAOpRxDispensationERPOutputParameters:
            return sd::epa_op_rx_dispensation_erp_output_parameters;
        case GEM_ERP_PR_AuditEvent:
            return sd::auditEvent;
        case GEM_ERP_PR_Binary:
            return sd::binary;
        case GEM_ERP_PR_Communication_DispReq:
            return sd::communicationDispReq;
        case GEM_ERP_PR_Communication_InfoReq:
            return sd::communicationInfoReq;
        case GEM_ERPCHRG_PR_Communication_ChargChangeReq:
            return sd::communicationChargChangeReq;
        case GEM_ERPCHRG_PR_Communication_ChargChangeReply:
            return sd::communicationChargChangeReply;
        case GEM_ERP_PR_Communication_Reply:
            return sd::communicationReply;
        case GEM_ERP_PR_Communication_Representative:
            return sd::communicationRepresentative;
        case GEM_ERP_PR_Composition:
            return sd::composition;
        case GEM_ERP_PR_Device:
            return sd::device;
        case GEM_ERP_PR_Digest:
            return sd::digest;
        case GEM_ERP_PR_Medication:
            return sd::medication;
        case GEM_ERP_PR_PAR_CloseOperation_Input:
            return sd::closeOperationInput;
        case GEM_ERP_PR_PAR_DispenseOperation_Input:
            return sd::dispenseOperationInput;
        case KBV_PR_ERP_Bundle:
            return sd::prescriptionItem;
        case KBV_PR_ERP_Composition:
            return sd::kbv_composition;
        case KBV_PR_ERP_Medication_Compounding:
            return sd::kbv_medication_compounding;
        case KBV_PR_ERP_Medication_FreeText:
            return sd::kbv_medication_free_text;
        case KBV_PR_ERP_Medication_Ingredient:
            return sd::kbv_medication_ingredient;
        case KBV_PR_ERP_Medication_PZN:
            return sd::kbv_medication_pzn;
        case KBV_PR_ERP_PracticeSupply:
            return sd::kbv_practice_supply;
        case KBV_PR_ERP_Prescription:
            return sd::kbv_medication_request;
        case KBV_PR_EVDGA_Bundle:
            return sd::kbv_pr_evdga_bundle;
        case KBV_PR_EVDGA_HealthAppRequest:
            return sd::kbv_pr_evdga_health_app_request;
        case KBV_PR_FOR_Coverage:
            return sd::kbv_for_coverage;
        case KBV_PR_FOR_Organization:
            return sd::kbv_for_organization;
        case KBV_PR_FOR_Patient:
            return sd::kbv_for_patient;
        case KBV_PR_FOR_Practitioner:
            return sd::kbv_for_practitioner;
        case KBV_PR_FOR_PractitionerRole:
            return sd::kbv_for_practitioner_role;
        case GEM_ERP_PR_MedicationDispense:
            return sd::medicationDispense;
        case GEM_ERP_PR_MedicationDispense_DiGA:
            return sd::medicationDispenseDiga;
        case MedicationDispenseBundle:
            return sd::medicationDispenseBundle;
        case GEM_ERP_PR_Bundle:
            return sd::receipt;
        case GEM_ERP_PR_Task:
            return sd::task;
        case GEM_ERPCHRG_PR_ChargeItem:
            return sd::chargeItem;
        case GEM_ERPCHRG_PR_Consent:
            return sd::consent;
        case DAV_PKV_PR_ERP_AbgabedatenBundle:
            return sd::dispenseItem;
        case Subscription:
            return sd::subscription;
        case OperationOutcome:
            return sd::operationoutcome;
        case OrganizationDirectory:
            return sd::organization_directory;
        case EPAMedicationPZNIngredient:
            return sd::epa_medication_pzn_ingredient;
    }
    Fail2("invalid value for ProfileType: " + std::to_string(static_cast<uintmax_t>(profileType)), std::logic_error);
}

std::optional<model::ProfileType> model::findProfileType(std::string_view profileUrl)
{
    using enum ProfileType;
    namespace sd = resource::structure_definition;
    static const std::map<std::string_view, ProfileType> profileUrlToTypeMap{
        {sd::auditEvent, GEM_ERP_PR_AuditEvent},
        {sd::binary, GEM_ERP_PR_Binary},
        {sd::communicationDispReq, GEM_ERP_PR_Communication_DispReq},
        {sd::communicationInfoReq, GEM_ERP_PR_Communication_InfoReq},
        {sd::communicationChargChangeReq, GEM_ERPCHRG_PR_Communication_ChargChangeReq},
        {sd::communicationChargChangeReply, GEM_ERPCHRG_PR_Communication_ChargChangeReply},
        {sd::communicationReply, GEM_ERP_PR_Communication_Reply},
        {sd::communicationRepresentative, GEM_ERP_PR_Communication_Representative},
        {sd::composition, GEM_ERP_PR_Composition},
        {sd::device, GEM_ERP_PR_Device},
        {sd::digest, GEM_ERP_PR_Digest},
        {sd::prescriptionItem, KBV_PR_ERP_Bundle},
        {sd::kbv_composition, KBV_PR_ERP_Composition},
        {sd::kbv_medication_compounding, KBV_PR_ERP_Medication_Compounding},
        {sd::kbv_medication_free_text, KBV_PR_ERP_Medication_FreeText},
        {sd::kbv_medication_ingredient, KBV_PR_ERP_Medication_Ingredient},
        {sd::kbv_medication_pzn, KBV_PR_ERP_Medication_PZN},
        {sd::kbv_practice_supply, KBV_PR_ERP_PracticeSupply},
        {sd::kbv_medication_request, KBV_PR_ERP_Prescription},
        {sd::kbv_for_coverage, KBV_PR_FOR_Coverage},
        {sd::kbv_for_organization, KBV_PR_FOR_Organization},
        {sd::kbv_for_patient, KBV_PR_FOR_Patient},
        {sd::kbv_for_practitioner, KBV_PR_FOR_Practitioner},
        {sd::kbv_for_practitioner_role, KBV_PR_FOR_PractitionerRole},
        {sd::medicationDispense, GEM_ERP_PR_MedicationDispense},
        {sd::medicationDispenseDiga, GEM_ERP_PR_MedicationDispense_DiGA},
        {sd::medicationDispenseBundle, MedicationDispenseBundle},
        {sd::receipt, GEM_ERP_PR_Bundle},
        {sd::task, GEM_ERP_PR_Task},
        {sd::chargeItem, GEM_ERPCHRG_PR_ChargeItem},
        {sd::consent, GEM_ERPCHRG_PR_Consent},
        {sd::dispenseItem, DAV_PKV_PR_ERP_AbgabedatenBundle},
        {sd::subscription, Subscription},
        {sd::operationoutcome, OperationOutcome},
        {sd::organization_directory, OrganizationDirectory},
        {sd::epa_medication_pzn_ingredient, EPAMedicationPZNIngredient},
        {sd::kbv_pr_evdga_bundle, KBV_PR_EVDGA_Bundle},
    };
    const fhirtools::DefinitionKey key{profileUrl};
    if (auto profiType = profileUrlToTypeMap.find(key.url); profiType != profileUrlToTypeMap.end())
    {
        return profiType->second;
    }
    return std::nullopt;
}

std::optional<fhirtools::DefinitionKey> model::profileWithVersion(ProfileType profileType,
                                                                  const fhirtools::FhirStructureRepository& repoView)
{
    std::optional profileStr = profile(profileType);
    if (! profileStr.has_value())
    {
        return std::nullopt;
    }
    if (const auto* def = repoView.findStructure(fhirtools::DefinitionKey{*profileStr}))
    {
        return fhirtools::DefinitionKey{def->url(), def->version()};
    }
    return std::nullopt;
}


std::string model::profileList(const fhirtools::FhirStructureRepository& repoView,
                               std::initializer_list<ProfileType> types)
{
    std::ostringstream result;
    std::string_view sep;
    for (const ProfileType type : types)
    {
        if (auto pv = profileWithVersion(type, repoView))
        {
            result << sep << to_string(*pv);
            sep = ", ";
        }
    }
    return std::move(result).str();
}
