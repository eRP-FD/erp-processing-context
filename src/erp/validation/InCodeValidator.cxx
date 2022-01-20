/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/validation/InCodeValidator.hxx"
#include "erp/common/HttpStatus.hxx"
#include "erp/model/Resource.hxx"
#include "erp/util/Expect.hxx"
#include "erp/validation/KbvBundleValidator.hxx"
#include "erp/validation/KbvCompositionValidator.hxx"
#include "erp/validation/KbvCoverageValidator.hxx"
#include "erp/validation/KbvMedicationCompoundingValidator.hxx"
#include "erp/validation/KbvMedicationFreeTextValidator.hxx"
#include "erp/validation/KbvMedicationIngredientValidator.hxx"
#include "erp/validation/KbvMedicationPZNValidator.hxx"
#include "erp/validation/KbvMedicationRequestValidator.hxx"
#include "erp/validation/KbvOrganizationValidator.hxx"
#include "erp/validation/KbvPatientValidator.hxx"
#include "erp/validation/KbvPractitionerValidator.hxx"
#include "erp/validation/KbvPracticeSupplyValidator.hxx"
#include "erp/validation/XmlValidator.hxx"


InCodeValidator::InCodeValidator()
{
    mMandatoryValidation = {SchemaType::KBV_PR_ERP_Bundle,
                            SchemaType::KBV_PR_ERP_Medication_Compounding,
                            SchemaType::KBV_PR_ERP_Composition,
                            SchemaType::KBV_PR_ERP_Medication_FreeText,
                            SchemaType::KBV_PR_ERP_Medication_Ingredient,
                            SchemaType::KBV_PR_ERP_Medication_PZN,
                            SchemaType::KBV_PR_ERP_PracticeSupply,
                            SchemaType::KBV_PR_ERP_Prescription,
                            SchemaType::KBV_PR_FOR_Coverage,
                            SchemaType::KBV_PR_FOR_HumanName,
                            SchemaType::KBV_PR_FOR_Organization,
                            SchemaType::KBV_PR_FOR_Patient,
                            SchemaType::KBV_PR_FOR_Practitioner,
                            SchemaType::KBV_PR_FOR_PractitionerRole};
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_1),
                               std::make_unique<KbvBundleValidator_V1_0_1>());
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_ERP_Composition, model::ResourceVersion::KbvItaErp::v1_0_1),
                               std::make_unique<KbvCompositionValidator_V1_0_1>());
    mKbvValidators.try_emplace(
        KeyKbv(SchemaType::KBV_PR_ERP_Medication_Compounding, model::ResourceVersion::KbvItaErp::v1_0_1),
        std::make_unique<KbvMedicationCompoundingValidator_V1_0_1>());
    mKbvValidators.try_emplace(
        KeyKbv(SchemaType::KBV_PR_ERP_Medication_FreeText, model::ResourceVersion::KbvItaErp::v1_0_1),
        std::make_unique<KbvMedicationFreeTextValidator_V1_0_1>());
    mKbvValidators.try_emplace(
        KeyKbv(SchemaType::KBV_PR_ERP_Medication_Ingredient, model::ResourceVersion::KbvItaErp::v1_0_1),
        std::make_unique<KbvMedicationIngredientValidator_V1_0_1>());
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_ERP_Medication_PZN, model::ResourceVersion::KbvItaErp::v1_0_1),
                               std::make_unique<KbvMedicationPZNValidator_V1_0_1>());
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_FOR_Coverage, model::ResourceVersion::KbvItaErp::v1_0_1),
                               std::make_unique<KbvCoverageValidator_V1_0_1>());
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_ERP_Prescription, model::ResourceVersion::KbvItaErp::v1_0_1),
                               std::make_unique<KbvMedicationRequestValidator_V1_0_1>());
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_FOR_Organization, model::ResourceVersion::KbvItaErp::v1_0_1),
                               std::make_unique<KbvOrganizationValidator_V1_0_1>());
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_FOR_Patient, model::ResourceVersion::KbvItaErp::v1_0_1),
                               std::make_unique<KbvPatientValidator_V1_0_1>());
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_FOR_Practitioner, model::ResourceVersion::KbvItaErp::v1_0_1),
                               std::make_unique<KbvPractitionerValidator_V1_0_1>());
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_ERP_PracticeSupply, model::ResourceVersion::KbvItaErp::v1_0_1),
                               std::make_unique<KbvPracticeSupplyValidator_V1_0_1>());

    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2),
                               std::make_unique<KbvBundleValidator_V1_0_2>());
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_ERP_Composition, model::ResourceVersion::KbvItaErp::v1_0_2),
                               std::make_unique<KbvCompositionValidator_V1_0_2>());
    mKbvValidators.try_emplace(
        KeyKbv(SchemaType::KBV_PR_ERP_Medication_Compounding, model::ResourceVersion::KbvItaErp::v1_0_2),
        std::make_unique<KbvMedicationCompoundingValidator_V1_0_2>());
    mKbvValidators.try_emplace(
        KeyKbv(SchemaType::KBV_PR_ERP_Medication_FreeText, model::ResourceVersion::KbvItaErp::v1_0_2),
        std::make_unique<KbvMedicationFreeTextValidator_V1_0_2>());
    mKbvValidators.try_emplace(
        KeyKbv(SchemaType::KBV_PR_ERP_Medication_Ingredient, model::ResourceVersion::KbvItaErp::v1_0_2),
        std::make_unique<KbvMedicationIngredientValidator_V1_0_2>());
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_ERP_Medication_PZN, model::ResourceVersion::KbvItaErp::v1_0_2),
                               std::make_unique<KbvMedicationPZNValidator_V1_0_2>());
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_FOR_Coverage, model::ResourceVersion::KbvItaErp::v1_0_2),
                               std::make_unique<KbvCoverageValidator_V1_0_2>());
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_ERP_Prescription, model::ResourceVersion::KbvItaErp::v1_0_2),
                               std::make_unique<KbvMedicationRequestValidator_V1_0_2>());
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_FOR_Organization, model::ResourceVersion::KbvItaErp::v1_0_2),
                               std::make_unique<KbvOrganizationValidator_V1_0_2>());
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_FOR_Patient, model::ResourceVersion::KbvItaErp::v1_0_2),
                               std::make_unique<KbvPatientValidator_V1_0_2>());
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_FOR_Practitioner, model::ResourceVersion::KbvItaErp::v1_0_2),
                               std::make_unique<KbvPractitionerValidator_V1_0_2>());
    mKbvValidators.try_emplace(KeyKbv(SchemaType::KBV_PR_ERP_PracticeSupply, model::ResourceVersion::KbvItaErp::v1_0_2),
                               std::make_unique<KbvPracticeSupplyValidator_V1_0_2>());
}


void InCodeValidator::validate(const model::ResourceBase& resource, SchemaType schemaType,
                               model::ResourceVersion::KbvItaErp version, const XmlValidator& xmlValidator) const
{
    if (mMandatoryValidation.count(schemaType) > 0)
    {
        const auto& candidate = mKbvValidators.find(KeyKbv(schemaType, version));
        ErpExpect(candidate != mKbvValidators.end(), HttpStatus::BadRequest,
                  "invalid profile type|version " + std::string(magic_enum::enum_name(schemaType)) + "|" +
                      std::string(model::ResourceVersion::v_str(version)));
        Expect(candidate->second, "nullptr registered as resource validator");
        candidate->second->validate(resource, xmlValidator, *this);
    }
}


void InCodeValidator::validate(const model::ResourceBase& resource, SchemaType schemaType,
                               model::ResourceVersion::DeGematikErezeptWorkflowR4 version,
                               const XmlValidator& xmlValidator) const
{
    if (mMandatoryValidation.count(schemaType) > 0)
    {
        const auto& candidate = mGematikValidators.find(KeyGem(schemaType, version));
        ErpExpect(candidate != mGematikValidators.end(), HttpStatus::BadRequest,
                  "invalid profile type|version " + std::string(magic_enum::enum_name(schemaType)) + "|" +
                      std::string(model::ResourceVersion::v_str(version)));
        Expect(candidate->second, "nullptr registered as resource validator");
        candidate->second->validate(resource, xmlValidator, *this);
    }
}

void InCodeValidator::validate(const model::ResourceBase&, SchemaType schemaType,
                               model::ResourceVersion::NotProfiled, const XmlValidator&) const
{
    Expect(mMandatoryValidation.count(schemaType) == 0,
           "In-code validation not supported for un-profiled resources.");
}
