#include "ResourceNames.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/util/Expect.hxx"
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
        case Gem_erxAuditEvent:
            return sd::auditEvent;
        case Gem_erxBinary:
            return sd::binary;
        case Gem_erxCommunicationDispReq:
            return sd::communicationDispReq;
        case Gem_erxCommunicationInfoReq:
            return sd::communicationInfoReq;
        case Gem_erxCommunicationChargChangeReq:
            return sd::communicationChargChangeReq;
        case Gem_erxCommunicationChargChangeReply:
            return sd::communicationChargChangeReply;
        case Gem_erxCommunicationReply:
            return sd::communicationReply;
        case Gem_erxCommunicationRepresentative:
            return sd::communicationRepresentative;
        case Gem_erxCompositionElement:
            return sd::composition;
        case Gem_erxDevice:
            return sd::device;
        case Gem_erxDigest:
            return sd::digest;
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
        case Gem_erxMedicationDispense:
            return sd::medicationDispense;
        case MedicationDispenseBundle:
            return sd::medicationDispenseBundle;
        case Gem_erxReceiptBundle:
            return sd::receipt;
        case Gem_erxTask:
            return sd::task;
        case Gem_erxChargeItem:
            return sd::chargeItem;
        case Gem_erxConsent:
            return sd::consent;
        case DAV_DispenseItem:
            return sd::dispenseItem;
        case Subscription:
            return sd::subscription;
        case OperationOutcome:
            return sd::operationoutcome;
    };
    Fail2("invalid value for ProfileType: " + std::to_string(static_cast<uintmax_t>(profileType)), std::logic_error);
}

std::optional<model::ProfileType> model::findProfileType(std::string_view profileUrl)
{
    using enum ProfileType;
    namespace sd = resource::structure_definition;
    static const std::map<std::string_view, ProfileType> profileUrlToTypeMap{
        {sd::auditEvent, Gem_erxAuditEvent},
        {sd::binary, Gem_erxBinary},
        {sd::communicationDispReq, Gem_erxCommunicationDispReq},
        {sd::communicationInfoReq, Gem_erxCommunicationInfoReq},
        {sd::communicationChargChangeReq, Gem_erxCommunicationChargChangeReq},
        {sd::communicationChargChangeReply, Gem_erxCommunicationChargChangeReply},
        {sd::communicationReply, Gem_erxCommunicationReply},
        {sd::communicationRepresentative, Gem_erxCommunicationRepresentative},
        {sd::composition, Gem_erxCompositionElement},
        {sd::device, Gem_erxDevice},
        {sd::digest, Gem_erxDigest},
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
        {sd::medicationDispense, Gem_erxMedicationDispense},
        {sd::medicationDispenseBundle, MedicationDispenseBundle},
        {sd::receipt, Gem_erxReceiptBundle},
        {sd::task, Gem_erxTask},
        {sd::chargeItem, Gem_erxChargeItem},
        {sd::consent, Gem_erxConsent},
        {sd::dispenseItem, DAV_DispenseItem},
        {sd::subscription, Subscription},
        {sd::operationoutcome, OperationOutcome},
    };
    fhirtools::DefinitionKey key{profileUrl};
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
