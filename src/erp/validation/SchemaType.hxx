/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SCHEMATYPE_HXX
#define ERP_PROCESSING_CONTEXT_SCHEMATYPE_HXX

// these enum names must be in line with the schema filenames.
enum class SchemaType
{
    ActivateTaskParameters,
    CreateTaskParameters,
    Gem_erxAuditEvent,
    Gem_erxBinary,
    fhir, // general FHIR schema
    Gem_erxCommunicationDispReq,
    Gem_erxCommunicationInfoReq,
    Gem_erxCommunicationReply,
    Gem_erxCommunicationRepresentative,
    Gem_erxCompositionElement,
    Gem_erxDevice,
    KBV_PR_ERP_Bundle,
    KBV_PR_ERP_Composition,
    KBV_PR_ERP_Medication_Compounding,
    KBV_PR_ERP_Medication_FreeText,
    KBV_PR_ERP_Medication_Ingredient,
    KBV_PR_ERP_Medication_PZN,
    KBV_PR_ERP_PracticeSupply,
    KBV_PR_ERP_Prescription,
    KBV_PR_FOR_Coverage,
    KBV_PR_FOR_HumanName,
    KBV_PR_FOR_Organization,
    KBV_PR_FOR_Patient,
    KBV_PR_FOR_Practitioner,
    KBV_PR_FOR_PractitionerRole,
    Gem_erxMedicationDispense,
    MedicationDispenseBundle,
    Gem_erxOrganizationElement,
    Gem_erxReceiptBundle,
    Gem_erxTask,
    BNA_tsl,
    Gematik_tsl,
    Gem_erxChargeItem,
    Gem_erxConsent
};

#endif//ERP_PROCESSING_CONTEXT_SCHEMATYPE_HXX
