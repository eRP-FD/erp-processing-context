#include "shared/model/ProfileType.hxx"

#include "shared/util/Expect.hxx"

#include <cstdint>
#include <string>

namespace model
{
bool mandatoryMetaProfile(ProfileType profileType)
{

    switch(profileType)
    {
        using enum ProfileType;
        case fhir:// general FHIR schema
        case ActivateTaskParameters:
        case CreateTaskParameters:
        case Subscription:
        case OperationOutcome:
        case PatchChargeItemParameters:
        case ProvidePrescriptionErpOp:
        case EPAOpRxPrescriptionERPOutputParameters:
        case EPAOpRxDispensationERPOutputParameters:
        case CancelPrescriptionErpOp:
        case ProvideDispensationErpOp:
        case OrganizationDirectory:
        case EPAMedicationPZNIngredient:
            return false;
        case Gem_erxAuditEvent:
        case Gem_erxBinary:
        case Gem_erxCommunicationDispReq:
        case Gem_erxCommunicationInfoReq:
        case Gem_erxCommunicationChargChangeReq:
        case Gem_erxCommunicationChargChangeReply:
        case Gem_erxCommunicationReply:
        case Gem_erxCommunicationRepresentative:
        case Gem_erxCompositionElement:
        case Gem_erxDevice:
        case Gem_erxDigest:
        case GEM_ERP_PR_Medication:
        case GEM_ERP_PR_PAR_CloseOperation_Input:
        case GEM_ERP_PR_PAR_DispenseOperation_Input:
        case KBV_PR_ERP_Bundle:
        case KBV_PR_ERP_Composition:
        case KBV_PR_ERP_Medication_Compounding:
        case KBV_PR_ERP_Medication_FreeText:
        case KBV_PR_ERP_Medication_Ingredient:
        case KBV_PR_ERP_Medication_PZN:
        case KBV_PR_ERP_PracticeSupply:
        case KBV_PR_ERP_Prescription:
        case KBV_PR_FOR_Coverage:
        case KBV_PR_FOR_Organization:
        case KBV_PR_FOR_Patient:
        case KBV_PR_FOR_Practitioner:
        case KBV_PR_FOR_PractitionerRole:
        case Gem_erxMedicationDispense:
        case MedicationDispenseBundle:
        case Gem_erxReceiptBundle:
        case Gem_erxTask:
        case Gem_erxChargeItem:
        case Gem_erxConsent:
        case DAV_DispenseItem:
            return true;
    }
    Fail("invalid value for ProfileType: " + std::to_string(static_cast<uintmax_t>(profileType)));
}
}// namespace model
