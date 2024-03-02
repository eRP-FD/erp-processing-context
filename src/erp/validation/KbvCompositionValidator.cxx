/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/validation/KbvCompositionValidator.hxx"
#include "erp/model/KbvComposition.hxx"
#include "erp/validation/KbvValidationUtils.hxx"

namespace
{
enum class KbvCompositionConstraint
{
    erp_subjectAndPrescription,
    erp_coverageAndPrescription,
    erp_prescriptionOrPracticeSupply
};
std::string constraintMessage(KbvCompositionConstraint constraint)
{
    switch (constraint)
    {
        case KbvCompositionConstraint::erp_subjectAndPrescription:
            return "Referenz auf Patient nicht vorhanden, aber Pflicht bei Verordnung von Arzneimitteln";
        case KbvCompositionConstraint::erp_coverageAndPrescription:
            return "Referenz auf Krankenversicherungsverhältnis nicht vorhanden, aber Pflicht bei Verordnung von "
                   "Arzneimitteln";
        case KbvCompositionConstraint::erp_prescriptionOrPracticeSupply:
            return "Section \"Verordnung Arzneimittel\" und Section \"Verordnung Sprechstundenbedarf\" vorhanden, aber "
                   "nur eine von beiden zulässig.";
    }
    Fail("invalid KbvCompositionConstraint " + std::to_string(static_cast<std::uintmax_t>(constraint)));
}

std::string constraintError(KbvCompositionConstraint constraint,
                            const std::optional<std::string>& additional = std::nullopt)
{
    return KbvValidationUtils::constraintError(constraintMessage(constraint), additional);
}
}

void KbvCompositionValidator::validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                                       const InCodeValidator& inCodeValidator) const
{
    doValidate(dynamic_cast<const model::KbvComposition&>(resource), xmlValidator, inCodeValidator);
}

void KbvCompositionValidator_V1_0_2::doValidate(const model::KbvComposition& kbvComposition, const XmlValidator&,
                                                const InCodeValidator&) const
{
    KbvValidationUtils::checkKbvExtensionValueCoding(
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis", kbvComposition, false,
        {"https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"},
        {"00", "01", "04", "07", "10", "11", "14", "17"});
    KbvValidationUtils::checkKbvExtensionValueCoding(
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_PKV_Tariff", kbvComposition, false,
        {"https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PKV_TARIFF"}, {"01", "02", "03", "04"});

    erp_subjectAndPrescription(kbvComposition);
    erp_coverageAndPrescription(kbvComposition);
    erp_prescriptionOrPracticeSupply(kbvComposition);
    authorSlicing(kbvComposition);
}

void KbvCompositionValidator_V1_0_2::erp_subjectAndPrescription(const model::KbvComposition& kbvComposition) const
{
    // -erp-subjectAndPrescription
    // section.where(code.coding.code='Prescription').exists()
    //  implies subject.exists()
    try
    {
        const auto& section = kbvComposition.sectionEntry("Prescription");
        if (section)
        {
            const auto& subject = kbvComposition.subject();
            ErpExpect(subject.has_value(), HttpStatus::BadRequest,
                      constraintError(KbvCompositionConstraint::erp_subjectAndPrescription));
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest,
                               constraintError(KbvCompositionConstraint::erp_subjectAndPrescription), ex.what());
    }
}

void KbvCompositionValidator_V1_0_2::erp_coverageAndPrescription(const model::KbvComposition& kbvComposition) const
{
    // -erp-coverageAndPrescription
    // section.where(code.coding.code='Prescription').exists()
    //  implies section.where(code.coding.code='Coverage').exists()
    try
    {
        const auto& sectionPrescription = kbvComposition.sectionEntry("Prescription");
        if (sectionPrescription)
        {
            const auto& sectionCoverage = kbvComposition.sectionEntry("Coverage");
            ErpExpect(sectionCoverage.has_value(), HttpStatus::BadRequest,
                      constraintError(KbvCompositionConstraint::erp_coverageAndPrescription));
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest,
                               constraintError(KbvCompositionConstraint::erp_coverageAndPrescription), ex.what());
    }
}

void KbvCompositionValidator_V1_0_2::erp_prescriptionOrPracticeSupply(const model::KbvComposition& kbvComposition) const
{
    // -erp-prescriptionOrPracticeSupply
    // section.where(code.coding.code='Prescription').exists()
    //  xor section.where(code.coding.code='PracticeSupply').exists()
    try
    {
        const auto& sectionPrescription = kbvComposition.sectionEntry("Prescription");
        const auto& sectionPracticeSupply = kbvComposition.sectionEntry("PracticeSupply");
        if (sectionPrescription)
        {
            ErpExpect(! (sectionPrescription.has_value() && sectionPracticeSupply.has_value()), HttpStatus::BadRequest,
                      constraintError(KbvCompositionConstraint::erp_prescriptionOrPracticeSupply));
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest,
                               constraintError(KbvCompositionConstraint::erp_prescriptionOrPracticeSupply), ex.what());
    }
}

void KbvCompositionValidator_V1_0_2::authorSlicing(const model::KbvComposition& kbvComposition) const
{
    try
    {
        const auto& authorType = kbvComposition.authorType(0);
        ErpExpect(authorType, HttpStatus::BadRequest, "missing KBV_PR_ERP_Composition.author.type");
        const auto& authorType2 = kbvComposition.authorType(1);
        ErpExpect(authorType == "Practitioner" || authorType2 == "Practitioner", HttpStatus::BadRequest,
                  "missing author of type 'Practitioner'");
        checkAuthor(kbvComposition, *authorType, 0);
        if (authorType2)
        {
            checkAuthor(kbvComposition, *authorType2, 1);
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "error validating the KBV_PR_ERP_Composition.author field",
                               std::string(ex.what()));
    }
}

void KbvCompositionValidator_V1_0_2::checkAuthor(const model::KbvComposition& kbvComposition,
                                                 const std::string_view authorType, size_t idx) const
{
    if (authorType == "Practitioner")
    {
        const auto& reference = kbvComposition.authorReference(idx);
        ErpExpect(reference, HttpStatus::BadRequest, "missing KBV_PR_ERP_Composition.author.reference");
    }
    else if (authorType == "Device")
    {
        const auto& identifierSystem = kbvComposition.authorIdentifierSystem(idx);
        const auto& identifierValue = kbvComposition.authorIdentifierValue(idx);
        ErpExpect(identifierSystem, HttpStatus::BadRequest, "missing KBV_PR_ERP_Composition.author.identifier.system");
        ErpExpectWithDiagnostics(identifierSystem == "https://fhir.kbv.de/NamingSystem/KBV_NS_FOR_Pruefnummer",
                                 HttpStatus::BadRequest, "wrong KBV_PR_ERP_Composition.author.identifier.system",
                                 std::string(*identifierSystem));
        ErpExpect(identifierValue, HttpStatus::BadRequest, "missing KBV_PR_ERP_Composition.author.identifier.value");
    }
    else
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "invalid KBV_PR_ERP_Composition.author.type",
                               std::string(authorType));
    }
}
