/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/validation/KbvMedicationCompoundingValidator.hxx"
#include "erp/model/KbvMedicationCompounding.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/validation/KbvValidationUtils.hxx"

void KbvMedicationCompoundingValidator::validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                                                 const InCodeValidator& inCodeValidator) const
{
    doValidate(dynamic_cast<const model::KbvMedicationCompounding&>(resource), xmlValidator, inCodeValidator);
}

void KbvMedicationCompoundingValidator_V1_0_1::doValidate(
    const model::KbvMedicationCompounding& kbvMedicationCompounding, const XmlValidator&, const InCodeValidator&) const
{
    KbvValidationUtils::checkKbvExtensionValueCoding(
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Category", kbvMedicationCompounding, true,
        {"https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Medication_Category"}, {"00", "01", "02"});
    KbvValidationUtils::checkKbvExtensionValueBoolean(
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Vaccine", kbvMedicationCompounding, true);
    KbvValidationUtils::checkKbvExtensionValueString(
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_CompoundingInstruction",
        kbvMedicationCompounding, false, 60);
    KbvValidationUtils::checkKbvExtensionValueString(
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Packaging", kbvMedicationCompounding, false, 90);

    const auto* ingredientArray = kbvMedicationCompounding.ingredientArray();
    ErpExpect(ingredientArray && ingredientArray->IsArray(), HttpStatus::BadRequest, "missing ingredient array");
    for (auto item = ingredientArray->Begin(), end = ingredientArray->End(); item != end; ++item)
    {
        const auto ingredientResource = model::UnspecifiedResource::fromJson(*item);
        KbvValidationUtils::checkKbvExtensionValueString(
            "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Ingredient_Form", ingredientResource, false,
            30);

        static const rapidjson::Pointer strengthPointer(model::resource::ElementName::path("strength"));
        const auto* strength = strengthPointer.Get(*item);
        ErpExpect(strength, HttpStatus::BadRequest, "missing ingredient.strength");
        const auto strengthResource = model::UnspecifiedResource::fromJson(*strength);
        KbvValidationUtils::checkKbvExtensionValueString(
            "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Ingredient_Amount", strengthResource, false,
            20);
    }
}
