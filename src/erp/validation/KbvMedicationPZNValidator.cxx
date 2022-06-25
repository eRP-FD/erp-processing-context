/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/validation/KbvMedicationPZNValidator.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/model/Extension.hxx"
#include "erp/model/KbvMedicationPzn.hxx"
#include "erp/validation/KbvValidationUtils.hxx"

namespace
{
enum class KbvMedicationPZNConstraint
{
    erp_NormgroesseOderMenge,
    erp_begrenzungValue,
    erp_codeUndSystem,
    erp_begrenzungPznCode
};
std::string constraintMessage(KbvMedicationPZNConstraint constraint)
{
    switch (constraint)
    {
        case KbvMedicationPZNConstraint::erp_NormgroesseOderMenge:
            return "Packungsgröße oder Normgröße müssen mindestens angegeben sein";
        case KbvMedicationPZNConstraint::erp_begrenzungValue:
            return "Die Packungsgröße darf aus maximal 7 Zeichen bestehen";
        case KbvMedicationPZNConstraint::erp_codeUndSystem:
            return "Wenn ein Code eingegeben ist, muss auch das System hinterlegt sein.";
        case KbvMedicationPZNConstraint::erp_begrenzungPznCode:
            return "Der PZN-Code muss aus genau 8 Zeichen bestehen.";
    }
    Fail("invalid KbvMedicationPZNConstraint " + std::to_string(static_cast<std::uintmax_t>(constraint)));
}
std::string constraintError(KbvMedicationPZNConstraint constraint,
                            const std::optional<std::string>& additional = std::nullopt)
{
    return KbvValidationUtils::constraintError(constraintMessage(constraint), additional);
}
}

void KbvMedicationPZNValidator::validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                                         const InCodeValidator& inCodeValidator) const
{
    doValidate(dynamic_cast<const model::KbvMedicationPzn&>(resource), xmlValidator, inCodeValidator);
}

void KbvMedicationPZNValidator_V1_0_1::doValidate(const model::KbvMedicationPzn& kbvMedicationpzn, const XmlValidator&,
                                                  const InCodeValidator&) const
{
    KbvValidationUtils::checkKbvExtensionValueCoding(
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Category", kbvMedicationpzn, true,
        {"https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Medication_Category"}, {"00", "01", "02"});
    KbvValidationUtils::checkKbvExtensionValueBoolean(
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Vaccine", kbvMedicationpzn, true);
    KbvValidationUtils::checkKbvExtensionValueCode("http://fhir.de/StructureDefinition/normgroesse", kbvMedicationpzn,
                                                   false, {"KA", "KTP", "N1", "N2", "N3", "NB", "Sonstiges"});

    erp_NormgroesseOderMenge(kbvMedicationpzn);
    amountNumerator_erp_begrenzungValue(kbvMedicationpzn);
    amountNumerator_erp_codeUndSystem(kbvMedicationpzn);
    erp_begrenzungPznCode(kbvMedicationpzn);
}

void KbvMedicationPZNValidator_V1_0_1::erp_NormgroesseOderMenge(const model::KbvMedicationPzn& kbvMedicationpzn) const
{
    // -erp-NormgroesseOderMenge
    // extension('http://fhir.de/StructureDefinition/normgroesse').exists()
    //  or amount.exists()
    try
    {
        const auto& normgroesseExtension =
            kbvMedicationpzn.getExtension<model::Extension>("http://fhir.de/StructureDefinition/normgroesse");
        const auto& amount = kbvMedicationpzn.amountNumeratorValueAsString();
        ErpExpect(normgroesseExtension || amount, HttpStatus::BadRequest,
                  constraintError(KbvMedicationPZNConstraint::erp_NormgroesseOderMenge));
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest,
                               constraintError(KbvMedicationPZNConstraint::erp_NormgroesseOderMenge), ex.what());
    }
}

void KbvMedicationPZNValidator_V1_0_1::amountNumerator_erp_begrenzungValue(
    const model::KbvMedicationPzn& kbvMedicationPzn) const
{
    try
    {
        const auto& amountNumeratorValue = kbvMedicationPzn.amountNumeratorValueAsString();
        if (amountNumeratorValue)
        {
            ErpExpectWithDiagnostics(
                amountNumeratorValue->size() <= 7, HttpStatus::BadRequest,
                constraintError(KbvMedicationPZNConstraint::erp_begrenzungValue),
                std::string(*amountNumeratorValue));
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest,
                               constraintError(KbvMedicationPZNConstraint::erp_begrenzungValue), ex.what());
    }
}

void KbvMedicationPZNValidator_V1_0_1::amountNumerator_erp_codeUndSystem(
    const model::KbvMedicationPzn& kbvMedicationPzn) const
{
    try
    {
        const auto& code = kbvMedicationPzn.amountNumeratorCode();
        if (code)
        {
            const auto& system = kbvMedicationPzn.amountNumeratorSystem();
            ErpExpect(system, HttpStatus::BadRequest,
                      constraintError(KbvMedicationPZNConstraint::erp_codeUndSystem));
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest,
                               constraintError(KbvMedicationPZNConstraint::erp_codeUndSystem), ex.what());
    }
}

void KbvMedicationPZNValidator_V1_0_1::erp_begrenzungPznCode(const model::KbvMedicationPzn& kbvMedicationpzn) const
{
    A_22925.start("PZN length constraint check");
    const auto* errorMessageFromAfo = "Länge PZN unzulässig (muss 8-stellig sein)";
    try
    {
        const auto pznString = kbvMedicationpzn.pzn();
        ErpExpect(pznString.size() == 8, HttpStatus::BadRequest, errorMessageFromAfo);
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, errorMessageFromAfo, ex.what());
    }
    A_22925.finish();
}
