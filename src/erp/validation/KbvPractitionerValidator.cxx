/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/validation/KbvPractitionerValidator.hxx"
#include "erp/model/KbvPractitioner.hxx"
#include "erp/validation/KbvValidationUtils.hxx"


void KbvPractitionerValidator::validate(const model::ResourceBase& resource, const XmlValidator&,
                                        const InCodeValidator&) const
{
    doValidate(dynamic_cast<const model::KbvPractitioner&>(resource));
}

void KbvPractitionerValidator_V1_0_1::doValidate(const model::KbvPractitioner& kbvPractitioner) const
{
    identifierSlicing(kbvPractitioner);
    KbvValidationUtils::checkFamilyName(kbvPractitioner.name_family());
    KbvValidationUtils::checkNamePrefix(kbvPractitioner.name_prefix());
    KbvValidationUtils::hum_1_2_3(kbvPractitioner.name_family(), kbvPractitioner.nameFamily());
    KbvValidationUtils::hum_4(kbvPractitioner.name_prefix(), kbvPractitioner.namePrefix());
    qualificationSlicing(kbvPractitioner);
}

void KbvPractitionerValidator_V1_0_1::identifierSlicing(const model::KbvPractitioner& kbvPractitioner) const
{
    const auto& typeCodingCode = kbvPractitioner.identifierTypeCodingCode();
    const auto& typeCodingSystem = kbvPractitioner.identifierTypeCodingSystem();
    const auto& use = kbvPractitioner.identifierUse();
    const auto& system = kbvPractitioner.identifierSystem();
    const auto& value = kbvPractitioner.identifierValue();
    if (typeCodingCode)
    {
        if (use)
        {
            ErpExpectWithDiagnostics(use == "official", HttpStatus::BadRequest,
                                     "identifier.use must be 'official' if present", std::string(*use));
        }
        ErpExpect(system, HttpStatus::BadRequest, "missing mandatory field identifier.system");
        ErpExpect(value, HttpStatus::BadRequest, "missing mandatory field identifier.value");
        if (typeCodingCode == "LANR")
        {
            ErpExpectWithDiagnostics(
                typeCodingSystem == "http://terminology.hl7.org/CodeSystem/v2-0203", HttpStatus::BadRequest,
                "identifier.type.coding.system must be 'http://terminology.hl7.org/CodeSystem/v2-0203'",
                std::string(typeCodingSystem.value_or("")));
            ErpExpectWithDiagnostics(
                system == "https://fhir.kbv.de/NamingSystem/KBV_NS_Base_ANR", HttpStatus::BadRequest,
                "identifier.system must be 'https://fhir.kbv.de/NamingSystem/KBV_NS_Base_ANR'", std::string(*system));
            KbvValidationUtils::checkLanr(*value);
        }
        else if (typeCodingCode == "ZANR")
        {
            ErpExpectWithDiagnostics(
                typeCodingSystem == "http://fhir.de/CodeSystem/identifier-type-de-basis", HttpStatus::BadRequest,
                "identifier.type.coding.system must be 'http://fhir.de/CodeSystem/identifier-type-de-basis'",
                std::string(typeCodingSystem.value_or("")));
            ErpExpectWithDiagnostics(
                system == "http://fhir.de/NamingSystem/kzbv/zahnarztnummer", HttpStatus::BadRequest,
                "identifier.system must be 'http://fhir.de/NamingSystem/kzbv/zahnarztnummer'", std::string(*system));
            KbvValidationUtils::checkZanr(*value);
        }
    }
}

static void checkQualification(const std::optional<std::string_view>& qualificationCodeText,
                               const std::optional<std::string_view>& qualificationCodeCodingSystem,
                               const std::optional<std::string_view>& qualificationCodeCodingCode)
{
    if (! qualificationCodeText && qualificationCodeCodingSystem == "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Qualification_Type")
    {
        const std::set<std::string_view> codes = {"00", "01", "02", "03", "04"};
        ErpExpectWithDiagnostics(
            codes.count(*qualificationCodeCodingCode), HttpStatus::BadRequest,
            "qualification.code.coding.code not in allowed set [\"00\", \"01\", \"02\", \"03\", \"04\"]",
            std::string(*qualificationCodeCodingCode));
    }
}

void KbvPractitionerValidator_V1_0_1::qualificationSlicing(const model::KbvPractitioner& kbvPractitioner) const
{
    // Slicing on code.text(Exists)
    const auto& text1 = kbvPractitioner.qualificationText(0);
    const auto& text2 = kbvPractitioner.qualificationText(1);
    const auto& codeCodingSystem1 = kbvPractitioner.qualificationCodeCodingSystem(0);
    const auto& codeCodingSystem2 = kbvPractitioner.qualificationCodeCodingSystem(1);
    const auto& codeCodingCode1 = kbvPractitioner.qualificationCodeCodingCode(0);
    const auto& codeCodingCode2 = kbvPractitioner.qualificationCodeCodingCode(1);
    checkQualification(text1, codeCodingSystem1, codeCodingCode1);
    checkQualification(text2, codeCodingSystem2, codeCodingCode2);
}
