/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/validation/KbvMedicationIngredientValidator.hxx"
#include "erp/model/Extension.hxx"
#include "erp/model/KbvMedicationIngredient.hxx"
#include "erp/validation/KbvValidationUtils.hxx"

namespace
{
enum class KbvMedicationIngredientConstraint
{
    erp_PackungOderNormgroesse,
    amountNumerator_erp_begrenzungValue,
    amountNumerator_erp_codeUndSystem,
    ingredientStrengthNumerator_erp_begrenzungValue
};
std::string constraintMessage(KbvMedicationIngredientConstraint constraint)
{
    switch (constraint)
    {
        case KbvMedicationIngredientConstraint::erp_PackungOderNormgroesse:
            return "Packungsgröße oder Normgröße müssen angegeben sein";
        case KbvMedicationIngredientConstraint::amountNumerator_erp_begrenzungValue:
            return "Die Packungsgröße darf aus maximal 7 Zeichen bestehen";
        case KbvMedicationIngredientConstraint::amountNumerator_erp_codeUndSystem:
            return "Wenn ein Code eingegeben ist, muss auch das System hinterlegt sein.";
        case KbvMedicationIngredientConstraint::ingredientStrengthNumerator_erp_begrenzungValue:
            return "Die Wirkstärke darf aus maximal 15 Zeichen bestehen";
    }
    Fail("invalid KbvMedicationIngredientConstraint " + std::to_string(static_cast<std::uintmax_t>(constraint)));
}
std::string constraintError(KbvMedicationIngredientConstraint constraint,
                            const std::optional<std::string>& additional = std::nullopt)
{
    return KbvValidationUtils::constraintError(constraintMessage(constraint), additional);
}
}

void KbvMedicationIngredientValidator::validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                                                const InCodeValidator& inCodeValidator) const
{
    doValidate(dynamic_cast<const model::KbvMedicationIngredient&>(resource), xmlValidator, inCodeValidator);
}

void KbvMedicationIngredientValidator_V1_0_2::doValidate(const model::KbvMedicationIngredient& kbvMedicationCompounding,
                                                         const XmlValidator&, const InCodeValidator&) const
{
    KbvValidationUtils::checkKbvExtensionValueCoding(
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Category", kbvMedicationCompounding, true,
        {"https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Medication_Category"}, {"00", "01", "02"});
    KbvValidationUtils::checkKbvExtensionValueBoolean(
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Vaccine", kbvMedicationCompounding, true);
    KbvValidationUtils::checkKbvExtensionValueCode("http://fhir.de/StructureDefinition/normgroesse",
                                                   kbvMedicationCompounding, false,
                                                   {"KA", "KTP", "N1", "N2", "N3", "NB", "Sonstiges"});

    erp_PackungOderNormgroesse(kbvMedicationCompounding);
    amountNumerator_erp_begrenzungValue(kbvMedicationCompounding);
    amountNumerator_erp_codeUndSystem(kbvMedicationCompounding);
    ingredientStrengthNumerator_erp_begrenzungValue(kbvMedicationCompounding);
}

void KbvMedicationIngredientValidator_V1_0_2::erp_PackungOderNormgroesse(
    const model::KbvMedicationIngredient& kbvMedicationIngredient) const
{
    // -erp-PackungOderNormgroesse
    // extension('http://fhir.de/StructureDefinition/normgroesse').exists()
    //  or amount.exists()
    try
    {
        const auto& normgroesseExtension =
            kbvMedicationIngredient.getExtension<model::Extension>("http://fhir.de/StructureDefinition/normgroesse");
        const auto& amount = kbvMedicationIngredient.amountNumeratorValueAsString();
        ErpExpect(normgroesseExtension || amount, HttpStatus::BadRequest,
                  constraintError(KbvMedicationIngredientConstraint::erp_PackungOderNormgroesse));
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest,
                               constraintError(KbvMedicationIngredientConstraint::erp_PackungOderNormgroesse),
                               ex.what());
    }
}

void KbvMedicationIngredientValidator_V1_0_2::amountNumerator_erp_begrenzungValue(
    const model::KbvMedicationIngredient& kbvMedicationIngredient) const
{
    // -erp-begrenzungValue
    // value.toString().length()<=7
    try
    {
        const auto& amountNumeratorValue = kbvMedicationIngredient.amountNumeratorValueAsString();
        if (amountNumeratorValue)
        {
            ErpExpectWithDiagnostics(
                amountNumeratorValue->size() <= 7, HttpStatus::BadRequest,
                constraintError(KbvMedicationIngredientConstraint::amountNumerator_erp_begrenzungValue),
                std::string(*amountNumeratorValue));
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest,
                               constraintError(KbvMedicationIngredientConstraint::amountNumerator_erp_begrenzungValue),
                               ex.what());
    }
}

void KbvMedicationIngredientValidator_V1_0_2::amountNumerator_erp_codeUndSystem(
    const model::KbvMedicationIngredient& kbvMedicationIngredient) const
{
    // -erp-codeUndSystem
    // code.exists() implies system.exists()
    try
    {
        const auto& code = kbvMedicationIngredient.amountNumeratorCode();
        if (code)
        {
            const auto& system = kbvMedicationIngredient.amountNumeratorSystem();
            ErpExpect(system, HttpStatus::BadRequest,
                      constraintError(KbvMedicationIngredientConstraint::amountNumerator_erp_codeUndSystem));
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest,
                               constraintError(KbvMedicationIngredientConstraint::amountNumerator_erp_codeUndSystem),
                               ex.what());
    }
}

void KbvMedicationIngredientValidator_V1_0_2::ingredientStrengthNumerator_erp_begrenzungValue(
    const model::KbvMedicationIngredient& kbvMedicationIngredient) const
{
    // -erp-begrenzungValue
    // value.toString().length()<=15
    try
    {
        const auto& value = kbvMedicationIngredient.ingredientStrengthNumeratorValueAsString();
        if (value)
        {
            ErpExpectWithDiagnostics(
                value->size() <= 15, HttpStatus::BadRequest,
                constraintError(KbvMedicationIngredientConstraint::ingredientStrengthNumerator_erp_begrenzungValue),
                std::string(*value));
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(
            HttpStatus::BadRequest,
            constraintError(KbvMedicationIngredientConstraint::ingredientStrengthNumerator_erp_begrenzungValue),
            ex.what());
    }
}
