/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/validation/KbvPatientValidator.hxx"
#include "erp/model/Extension.hxx"
#include "erp/model/Patient.hxx"
#include "erp/model/Reference.hxx"
#include "erp/validation/KbvValidationUtils.hxx"


void KbvPatientValidator::validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                                   const InCodeValidator& inCodeValidator) const
{
    doValidate(dynamic_cast<const model::Patient&>(resource), xmlValidator, inCodeValidator);
}


void KbvPatientValidator_V1_0_2::doValidate(const model::Patient& patient, const XmlValidator&,
                                            const InCodeValidator&) const
{
    const auto& birthDate = patient.birthDate();
    if (birthDate)
    {
        KbvValidationUtils::checkKbvExtensionValueCode("http://hl7.org/fhir/StructureDefinition/data-absent-reason",
                                                       *birthDate, false, {"unknown"});
    }

    KbvValidationUtils::checkFamilyName(patient.name_family());
    KbvValidationUtils::checkNamePrefix(patient.name_prefix());
    KbvValidationUtils::hum_1_2_3(patient.name_family(), patient.nameFamily());
    KbvValidationUtils::hum_4(patient.name_prefix(), patient.namePrefix());

    identifierSlicing(patient);
    addressSlicing(patient);

    const auto& line0 = patient.addressLine(0);
    const auto& line1 = patient.addressLine(1);
    const auto& _line0 = patient.address_line(0);
    const auto& _line1 = patient.address_line(1);
    KbvValidationUtils::add1(line0, _line0);
    KbvValidationUtils::add1(line1, _line1);
    KbvValidationUtils::add2(line0, _line0);
    KbvValidationUtils::add2(line1, _line1);
    KbvValidationUtils::add3(line0, _line0);
    KbvValidationUtils::add3(line1, _line1);
    KbvValidationUtils::add4(line0, _line0, patient.addressType().value_or(""));
    KbvValidationUtils::add4(line1, _line1, patient.addressType().value_or(""));
    KbvValidationUtils::add5(line0, _line0);
    KbvValidationUtils::add5(line1, _line1);
    KbvValidationUtils::add6(_line0);
    KbvValidationUtils::add6(_line1);
    KbvValidationUtils::add7(patient.address(), line0, line1);
}

void KbvPatientValidator_V1_0_2::identifierSlicing(const model::Patient& patient) const
{
    // Note: Closed Slicing.
    if (patient.hasIdentifier())
    {
        const auto& identifierUse = patient.identifierUse();
        const auto& identifierTypeCodingCode = patient.identifierTypeCodingCode();
        const auto& identifierTypeCodingSystem = patient.identifierTypeCodingSystem();
        const auto& identifierSystem = patient.identifierSystem();
        const auto& identifierValue = patient.identifierValue();
        ErpExpect(identifierTypeCodingCode, HttpStatus::BadRequest, "missing identifier.type.coding.code");
        ErpExpect(identifierTypeCodingSystem, HttpStatus::BadRequest, "missing identifier.type.coding.system");
        ErpExpectWithDiagnostics(identifierTypeCodingCode == "GKV" || identifierTypeCodingCode == "PKV" ||
                                     identifierTypeCodingCode == "kvk",
                                 HttpStatus::BadRequest, "identifier.type.coding.code must be GKV|PKV|kvk",
                                 std::string(*identifierTypeCodingCode));
        ErpExpect(identifierValue, HttpStatus::BadRequest, "mandatory identifier.value not set");
        if (identifierTypeCodingCode == "GKV")
        {
            identifierSlicingGKV(identifierUse, identifierTypeCodingSystem, identifierSystem, identifierValue);
        }
        else if (identifierTypeCodingCode == "PKV")
        {
            identifierSlicingPKV(identifierUse, identifierTypeCodingSystem);
            const auto& assignerDisplay = patient.identifierAssignerDisplay();
            ErpExpect(assignerDisplay, HttpStatus::BadRequest, "missing mandatory identifier.assigner.display");
            const auto& assigner = patient.identifierAssigner();
            pkvAssigner(assigner);
        }
        else if (identifierTypeCodingCode == "kvk")
        {
            identifierSlicingKvk(identifierUse, identifierTypeCodingSystem, identifierSystem);
        }
    }
}

void KbvPatientValidator_V1_0_2::pkvAssigner(const std::optional<model::Reference>& assigner) const
{
    ErpExpect(assigner, HttpStatus::BadRequest, "missing mandatory identifier.assigner");
    const auto& assignerIdentifierUse = assigner->identifierUse();
    const auto& assignerIdentifierTypeCodingSystem = assigner->identifierTypeCodingSystem();
    const auto& assignerIdentifierTypeCodingCode = assigner->identifierTypeCodingCode();
    const auto& assignerIdentifierSystem = assigner->identifierSystem();
    const auto& assignerIdentifierValue = assigner->identifierValue();
    if (assignerIdentifierUse)
    {
        ErpExpectWithDiagnostics(assignerIdentifierUse == "official", HttpStatus::BadRequest,
                                 "identifier.assigner.identifier.use must be 'official'",
                                 std::string(*assignerIdentifierUse));
    }
    if (assignerIdentifierTypeCodingSystem)
    {
        ErpExpectWithDiagnostics(assignerIdentifierTypeCodingSystem == "http://terminology.hl7.org/CodeSystem/v2-0203",
                                 HttpStatus::BadRequest,
                                 "identifier.assigner.identifier.type.coding.system must be "
                                 "'http://terminology.hl7.org/CodeSystem/v2-0203'",
                                 std::string(*assignerIdentifierTypeCodingSystem));
    }
    if (assignerIdentifierTypeCodingCode)
    {
        ErpExpectWithDiagnostics(assignerIdentifierTypeCodingCode == "XX", HttpStatus::BadRequest,
                                 "identifier.assigner.identifier.type.coding.code must be 'XX'",
                                 std::string(*assignerIdentifierTypeCodingCode));
    }
    if (assignerIdentifierSystem)
    {
        ErpExpectWithDiagnostics(
            assignerIdentifierSystem == "http://fhir.de/NamingSystem/arge-ik/iknr", HttpStatus::BadRequest,
            "identifier.assigner.identifier.system must be 'http://fhir.de/NamingSystem/arge-ik/iknr'",
            std::string(assignerIdentifierSystem.value_or("")));
    }
    if (assignerIdentifierValue)
    {
        KbvValidationUtils::checkIknr(*assignerIdentifierValue);
    }
}


static void checkBothSlicingLine(const std::optional<model::UnspecifiedResource>& addressLine)
{
    KbvValidationUtils::checkKbvExtensionValueString("http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-streetName",
                                                     *addressLine, false, 46);
    KbvValidationUtils::checkKbvExtensionValueString(
        "http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-houseNumber", *addressLine, false, 9);
    KbvValidationUtils::checkKbvExtensionValueString(
        "http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-additionalLocator", *addressLine, false, 40);
}

void KbvPatientValidator_V1_0_2::addressSlicing(const model::Patient& patient) const
{
    const auto& addressType = patient.addressType();
    const auto& addressLine = patient.address_line(0);
    if (addressType == "both")
    {
        ErpExpect(addressLine, HttpStatus::BadRequest, "missing mandatory address.line");
        checkBothSlicingLine(addressLine);
        const auto& addressLine2 = patient.address_line(1);
        if (addressLine2)
        {
            checkBothSlicingLine(addressLine2);
        }
    }
    else if (addressType == "postal")
    {
        ErpExpect(addressLine, HttpStatus::BadRequest, "missing mandatory address.line");
        KbvValidationUtils::checkKbvExtensionValueString(
            "http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-postBox", *addressLine, true, 8);
        const auto& addressLine2 = patient.address_line(1);
        ErpExpect(! addressLine2, HttpStatus::BadRequest, "too many address.line");
    }
}

void KbvPatientValidator_V1_0_2::identifierSlicingGKV(const std::optional<std::string_view>& identifierUse,
                                                      const std::optional<std::string_view>& identifierTypeCodingSystem,
                                                      const std::optional<std::string_view>& identifierSystem,
                                                      const std::optional<std::string_view>& identifierValue) const
{
    if (identifierUse)
    {
        ErpExpectWithDiagnostics(identifierUse == "official", HttpStatus::BadRequest,
                                 "identifier.use must be 'official' if existent", std::string(*identifierUse));
    }
    ErpExpectWithDiagnostics(
        identifierTypeCodingSystem == "http://fhir.de/CodeSystem/identifier-type-de-basis", HttpStatus::BadRequest,
        "identifier.type.coding.system must be 'http://fhir.de/CodeSystem/identifier-type-de-basis'",
        std::string(identifierTypeCodingSystem.value_or("")));
    ErpExpectWithDiagnostics(identifierSystem == "http://fhir.de/NamingSystem/gkv/kvid-10", HttpStatus::BadRequest,
                             "identifier.system must be 'http://fhir.de/NamingSystem/gkv/kvid-10'",
                             std::string(identifierSystem.value_or("")));
    KbvValidationUtils::checkKvnr(*identifierValue);
}

void KbvPatientValidator_V1_0_2::identifierSlicingKvk(const std::optional<std::string_view>& identifierUse,
                                                      const std::optional<std::string_view>& identifierTypeCodingSystem,
                                                      const std::optional<std::string_view>& identifierSystem) const
{
    if (identifierUse)
    {
        ErpExpectWithDiagnostics(identifierUse == "usual" || identifierUse == "official" || identifierUse == "temp" ||
                                     identifierUse == "secondary" || identifierUse == "old",
                                 HttpStatus::BadRequest,
                                 "identifier.use must be 'usual|official|temp|secondary|old' if existent",
                                 std::string(*identifierUse));
    }
    ErpExpectWithDiagnostics(
        identifierTypeCodingSystem == "https://fhir.kbv.de/CodeSystem/KBV_CS_Base_identifier_type",
        HttpStatus::BadRequest,
        "identifier.type.coding.system must be 'https://fhir.kbv.de/CodeSystem/KBV_CS_Base_identifier_type'",
        std::string(identifierTypeCodingSystem.value_or("")));
    ErpExpectWithDiagnostics(identifierSystem == "http://fhir.de/NamingSystem/gkv/kvk-versichertennummer",
                             HttpStatus::BadRequest,
                             "identifier.system must be 'http://fhir.de/NamingSystem/gkv/kvk-versichertennummer'",
                             std::string(identifierSystem.value_or("")));
}

void KbvPatientValidator_V1_0_2::identifierSlicingPKV(
    const std::optional<std::string_view>& identifierUse,
    const std::optional<std::string_view>& identifierTypeCodingSystem) const
{
    if (identifierUse)
    {
        ErpExpectWithDiagnostics(identifierUse == "secondary", HttpStatus::BadRequest,
                                 "identifier.use must be 'secondary' if existent", std::string(*identifierUse));
    }
    ErpExpectWithDiagnostics(
        identifierTypeCodingSystem == "http://fhir.de/CodeSystem/identifier-type-de-basis", HttpStatus::BadRequest,
        "identifier.type.coding.system must be 'http://fhir.de/CodeSystem/identifier-type-de-basis'",
        std::string(identifierTypeCodingSystem.value_or("")));
}
