#ifndef ERP_PROCESSING_CONTEXT_PROFILETYPE_HXX
#define ERP_PROCESSING_CONTEXT_PROFILETYPE_HXX

namespace model
{

enum class ProfileType
{
    ActivateTaskParameters,
    CreateTaskParameters,
    Gem_erxAuditEvent,
    Gem_erxBinary,
    fhir,// general FHIR schema
    Gem_erxCommunicationDispReq,
    Gem_erxCommunicationInfoReq,
    Gem_erxCommunicationChargChangeReq,
    Gem_erxCommunicationChargChangeReply,
    Gem_erxCommunicationReply,
    Gem_erxCommunicationRepresentative,
    Gem_erxCompositionElement,
    Gem_erxDevice,
    Gem_erxDigest,
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
    Gem_erxReceiptBundle,
    Gem_erxTask,
    Gem_erxChargeItem,
    Gem_erxConsent,
    PatchChargeItemParameters,
    DAV_DispenseItem,
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
