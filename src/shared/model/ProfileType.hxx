#ifndef ERP_PROCESSING_CONTEXT_PROFILETYPE_HXX
#define ERP_PROCESSING_CONTEXT_PROFILETYPE_HXX

namespace model
{

enum class ProfileType
{
    ActivateTaskParameters,
    CreateTaskParameters,
    GEM_ERP_PR_AuditEvent,
    GEM_ERP_PR_Binary,
    fhir,// general FHIR schema
    GEM_ERP_PR_Communication_DispReq,
    GEM_ERP_PR_Communication_InfoReq,
    GEM_ERPCHRG_PR_Communication_ChargChangeReq,
    GEM_ERPCHRG_PR_Communication_ChargChangeReply,
    GEM_ERP_PR_Communication_Reply,
    GEM_ERP_PR_Communication_Representative,
    GEM_ERP_PR_Composition,
    GEM_ERP_PR_Device,
    GEM_ERP_PR_Digest,
    GEM_ERP_PR_Medication,
    GEM_ERP_PR_MedicationDispense,
    GEM_ERP_PR_MedicationDispense_DiGA,
    GEM_ERP_PR_PAR_CloseOperation_Input,
    GEM_ERP_PR_PAR_DispenseOperation_Input,
    KBV_PR_ERP_Bundle,
    KBV_PR_ERP_Composition,
    KBV_PR_ERP_Medication_Compounding,
    KBV_PR_ERP_Medication_FreeText,
    KBV_PR_ERP_Medication_Ingredient,
    KBV_PR_ERP_Medication_PZN,
    KBV_PR_ERP_PracticeSupply,
    KBV_PR_ERP_Prescription,
    KBV_PR_EVDGA_Bundle,
    KBV_PR_EVDGA_HealthAppRequest,
    KBV_PR_FOR_Coverage,
    KBV_PR_FOR_Organization,
    KBV_PR_FOR_Patient,
    KBV_PR_FOR_Practitioner,
    KBV_PR_FOR_PractitionerRole,
    MedicationDispenseBundle,
    GEM_ERP_PR_Bundle,
    GEM_ERP_PR_Task,
    GEM_ERPCHRG_PR_ChargeItem,
    GEM_ERPCHRG_PR_Consent,
    PatchChargeItemParameters,
    DAV_PKV_PR_ERP_AbgabedatenBundle,
    Subscription,
    OperationOutcome,
    EPAOpRxPrescriptionERPOutputParameters,
    EPAOpRxDispensationERPOutputParameters,
    ProvidePrescriptionErpOp,
    CancelPrescriptionErpOp,
    ProvideDispensationErpOp,
    OrganizationDirectory,
    EPAMedicationPZNIngredient,
};

// these profiles define "meta.profile" as mandatory elements
bool mandatoryMetaProfile(ProfileType profileType);

}

#endif// ERP_PROCESSING_CONTEXT_PROFILETYPE_HXX
