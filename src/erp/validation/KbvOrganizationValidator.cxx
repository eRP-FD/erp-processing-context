/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/validation/KbvOrganizationValidator.hxx"
#include "erp/model/KbvOrganization.hxx"
#include "erp/validation/KbvValidationUtils.hxx"

namespace
{
void checkAddressLine(const std::optional<model::UnspecifiedResource>& line)
{
    if (line)
    {
        KbvValidationUtils::checkKbvExtensionValueString(
            "http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-streetName", *line, false, 46);
        KbvValidationUtils::checkKbvExtensionValueString(
            "http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-houseNumber", *line, false, 9);
        KbvValidationUtils::checkKbvExtensionValueString(
            "http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-additionalLocator", *line, false, 40);
    }
}
}

void KbvOrganizationValidator::validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                                        const InCodeValidator& inCodeValidator) const
{
    doValidate(dynamic_cast<const model::KbvOrganization&>(resource), xmlValidator, inCodeValidator);
}

void KbvOrganizationValidator_V1_0_1::doValidate(const model::KbvOrganization& kbvOrganization, const XmlValidator&,
                                                 const InCodeValidator&) const
{
    identifierSlicing(kbvOrganization);
    telecomSlicing(kbvOrganization);
    addressExtensions(kbvOrganization);
}

void KbvOrganizationValidator_V1_0_1::identifierSlicing(const model::KbvOrganization& kbvOrganization) const
{
    const auto& identifierUse = kbvOrganization.identifierUse();
    const auto& identifierTypeCodingCode = kbvOrganization.identifierTypeCodingCode();
    const auto& identifierTypeCodingSystem = kbvOrganization.identifierTypeCodingSystem();
    const auto& identifierSystem = kbvOrganization.identifierSystem();
    const auto& identifierValue = kbvOrganization.identifierValue();
    if (identifierTypeCodingCode == "XX" || identifierTypeCodingCode == "BSNR" || identifierTypeCodingCode == "KZVA")
    {
        if (identifierUse)
        {
            ErpExpectWithDiagnostics(identifierUse == "official", HttpStatus::BadRequest,
                                     "identifier.use must be 'official' if existent", std::string(*identifierUse));
        }
        ErpExpect(identifierValue, HttpStatus::BadRequest, "mandatory identifier.value not set");
        if (identifierTypeCodingCode == "XX")
        {
            ErpExpectWithDiagnostics(
                identifierTypeCodingSystem == "http://terminology.hl7.org/CodeSystem/v2-0203", HttpStatus::BadRequest,
                "identifier.type.coding.system must be 'http://terminology.hl7.org/CodeSystem/v2-0203'",
                std::string(identifierTypeCodingSystem.value_or("")));
            ErpExpectWithDiagnostics(identifierSystem == "http://fhir.de/NamingSystem/arge-ik/iknr",
                                     HttpStatus::BadRequest,
                                     "identifier.system must be 'http://fhir.de/NamingSystem/arge-ik/iknr'",
                                     std::string(identifierSystem.value_or("")));
            KbvValidationUtils::checkIknr(*identifierValue);
        }
        else if (identifierTypeCodingCode == "BSNR")
        {
            ErpExpectWithDiagnostics(
                identifierTypeCodingSystem == "http://terminology.hl7.org/CodeSystem/v2-0203", HttpStatus::BadRequest,
                "identifier.type.coding.system must be 'http://terminology.hl7.org/CodeSystem/v2-0203'",
                std::string(identifierTypeCodingSystem.value_or("")));
            ErpExpectWithDiagnostics(identifierSystem == "https://fhir.kbv.de/NamingSystem/KBV_NS_Base_BSNR",
                                     HttpStatus::BadRequest,
                                     "identifier.system must be 'https://fhir.kbv.de/NamingSystem/KBV_NS_Base_BSNR'",
                                     std::string(identifierSystem.value_or("")));
        }
        else if (identifierTypeCodingCode == "KZVA")
        {
            ErpExpectWithDiagnostics(
                identifierTypeCodingSystem == "http://fhir.de/CodeSystem/identifier-type-de-basis",
                HttpStatus::BadRequest,
                "identifier.type.coding.system must be 'http://fhir.de/CodeSystem/identifier-type-de-basis'",
                std::string(identifierTypeCodingSystem.value_or("")));
            ErpExpectWithDiagnostics(
                identifierSystem == "http://fhir.de/NamingSystem/kzbv/kzvabrechnungsnummer", HttpStatus::BadRequest,
                "identifier.system must be 'http://fhir.de/NamingSystem/kzbv/kzvabrechnungsnummer'",
                std::string(identifierSystem.value_or("")));
            KbvValidationUtils::checkZanr(*identifierValue);
        }
    }
}

void KbvOrganizationValidator_V1_0_1::telecomSlicing(const model::KbvOrganization& kbvOrganization) const
{
    const auto& telefon = kbvOrganization.telecom("phone");
    ErpExpect(telefon, HttpStatus::BadRequest, "missing mandatory telecom.phone");
}

void KbvOrganizationValidator_V1_0_1::addressExtensions(const model::KbvOrganization& kbvOrganization) const
{
    const auto& address = kbvOrganization.address();
    if (address)
    {
        const auto& _line0 = kbvOrganization.address_line(0);
        const auto& _line1 = kbvOrganization.address_line(1);
        checkAddressLine(_line0);
        checkAddressLine(_line1);
        const auto& line0 = kbvOrganization.addressLine(0);
        const auto& line1 = kbvOrganization.addressLine(1);
        KbvValidationUtils::add1(line0, _line0);
        KbvValidationUtils::add1(line1, _line1);
        KbvValidationUtils::add2(line0, _line0);
        KbvValidationUtils::add2(line1, _line1);
        KbvValidationUtils::add3(line0, _line0);
        KbvValidationUtils::add3(line1, _line1);
        KbvValidationUtils::add4(line0, _line0, kbvOrganization.addressType().value_or(""));
        KbvValidationUtils::add4(line1, _line1, kbvOrganization.addressType().value_or(""));
        KbvValidationUtils::add5(line0, _line0);
        KbvValidationUtils::add5(line1, _line1);
        KbvValidationUtils::add6(_line0);
        KbvValidationUtils::add6(_line1);
        KbvValidationUtils::add7(*address, line0, line1);
    }
}
