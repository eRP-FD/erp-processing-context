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
        case GEM_ERP_PR_AuditEvent:
        case GEM_ERP_PR_Binary:
        case GEM_ERP_PR_Communication_DispReq:
        case GEM_ERP_PR_Communication_InfoReq:
        case GEM_ERPCHRG_PR_Communication_ChargChangeReq:
        case GEM_ERPCHRG_PR_Communication_ChargChangeReply:
        case GEM_ERP_PR_Communication_Reply:
        case GEM_ERP_PR_Communication_Representative:
        case GEM_ERP_PR_Composition:
        case GEM_ERP_PR_Device:
        case GEM_ERP_PR_Digest:
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
        case KBV_PR_EVDGA_Bundle:
        case KBV_PR_EVDGA_HealthAppRequest:
        case KBV_PR_FOR_Coverage:
        case KBV_PR_FOR_Organization:
        case KBV_PR_FOR_Patient:
        case KBV_PR_FOR_Practitioner:
        case KBV_PR_FOR_PractitionerRole:
        case GEM_ERP_PR_MedicationDispense:
        case GEM_ERP_PR_MedicationDispense_DiGA:
        case MedicationDispenseBundle:
        case GEM_ERP_PR_Bundle:
        case GEM_ERP_PR_Task:
        case GEM_ERPCHRG_PR_ChargeItem:
        case GEM_ERPCHRG_PR_Consent:
        case DAV_PKV_PR_ERP_AbgabedatenBundle:
            return true;
    }
    Fail("invalid value for ProfileType: " + std::to_string(static_cast<uintmax_t>(profileType)));
}
}// namespace model
