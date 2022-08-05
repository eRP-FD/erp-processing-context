/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/validation/KbvValidationUtils.hxx"
#include "erp/common/HttpStatus.hxx"
#include "erp/model/Extension.hxx"
#include "erp/model/KbvCoverage.hxx"
#include "erp/model/Resource.hxx"
#include "fhirtools/model/Timestamp.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/String.hxx"



namespace
{
std::string setToString(const std::set<std::string_view>& valueSet)
{
    std::ostringstream oss;
    oss << "[";
    std::string_view sep;
    for (const auto& item : valueSet)
    {
        oss << sep << "\"" << item << "\"";
        sep = ", ";
    }
    oss << "]";
    return oss.str();
}

enum class KbvAddressLineConstraint
{
    add_1,
    add_2,
    add_3,
    add_4,
    add_5,
    add_6,
    add_7
};
std::string constraintMessage(KbvAddressLineConstraint constraint)
{
    switch (constraint)
    {
        case KbvAddressLineConstraint::add_1:
            return "Wenn die Extension 'Hausnummer' verwendet wird, muss auch Address.line gefüllt werden";
        case KbvAddressLineConstraint::add_2:
            return "Wenn die Extension 'Strasse' verwendet wird, muss auch Address.line gefüllt werden";
        case KbvAddressLineConstraint::add_3:
            return "Wenn die Extension 'Postfach' verwendet wird, muss auch Address.line gefüllt werden";
        case KbvAddressLineConstraint::add_4:
            return R"(Eine Postfach-Adresse darf nicht vom Type "physical" oder "both" sein.)";
        case KbvAddressLineConstraint::add_5:
            return "Wenn die Extension 'Adresszusatz' verwendet wird, muss auch Address.line gefüllt werden";
        case KbvAddressLineConstraint::add_6:
            return "Wenn die Extension 'Postfach' verwendet wird, dürfen die Extensions 'Strasse' und 'Hausnummer' "
                   "nicht verwendet werden";
        case KbvAddressLineConstraint::add_7:
            return "Wenn die Extension 'Precinct' (Stadtteil) verwendet wird, dann muss diese Information auch als "
                   "separates line-item abgebildet sein.";
    }
    Fail("invalid KbvAddressLineConstraint " + std::to_string(static_cast<std::uintmax_t>(constraint)));
}
std::string constraintError(KbvAddressLineConstraint constraint,
                            const std::optional<std::string>& additional = std::nullopt)
{
    return KbvValidationUtils::constraintError(constraintMessage(constraint), additional);
}
}


std::optional<model::Extension> KbvValidationUtils::checkGetExtension(const std::string_view& url,
                                                                      const model::ResourceBase& resource,
                                                                      bool mandatoryExtension)
{
    std::optional<model::Extension> extension;
    try
    {
        extension = resource.getExtension<model::Extension>(url);
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "error in extension " + std::string(url), ex.what());
    }
    if (mandatoryExtension)
    {
        ErpExpect(extension.has_value(), HttpStatus::BadRequest, "missing mandatory extension " + std::string(url));
    }
    return extension;
}

std::string KbvValidationUtils::constraintError(const std::string& constraintMessage,
                                                const std::optional<std::string>& additional)
{
    std::string message = "constraint violation: " + constraintMessage;
    if (additional)
    {
        message += " (" + additional.value() + ")";
    }
    return message;
}

void KbvValidationUtils::checkKbvExtensionValueCoding(const std::string_view& url, const model::ResourceBase& resource,
                                                      bool mandatoryExtension,
                                                      const std::set<std::string_view>& codeSystems,
                                                      const std::set<std::string_view>& valueSet)
{
    const auto& extension = checkGetExtension(url, resource, mandatoryExtension);
    if (extension.has_value())
    {
        const auto& system = extension->valueCodingSystem();
        ErpExpect(system.has_value(), HttpStatus::BadRequest,
                  "missing valueCoding.system in extension " + std::string(url));
        ErpExpectWithDiagnostics(codeSystems.count(*system), HttpStatus::BadRequest,
                                 "valueCode.system is not in the allowed set " + setToString(codeSystems) +
                                     " in extension " + std::string(url),
                                 std::string(*system));
        const auto& code = extension->valueCodingCode();
        ErpExpect(code.has_value(), HttpStatus::BadRequest,
                  "missing valueCoding.code in extension " + std::string(url));
        ErpExpectWithDiagnostics(valueSet.count(*code) > 0, HttpStatus::BadRequest,
                                 "value is not in the allowed set " + setToString(valueSet) + " in extension " +
                                     std::string(url),
                                 std::string(*code));
    }
}

void KbvValidationUtils::checkKbvExtensionValueBoolean(const std::string_view& url, const model::ResourceBase& resource,
                                                       bool mandatoryExtension)
{
    const auto& extension = checkGetExtension(url, resource, mandatoryExtension);
    if (extension.has_value())
    {
        ErpExpect(extension->valueBoolean().has_value(), HttpStatus::BadRequest,
                  "missing valueBoolean.value in extension " + std::string(url));
    }
}

void KbvValidationUtils::checkKbvExtensionValueString(const std::string_view& url, const model::ResourceBase& resource,
                                                      bool mandatoryExtension, std::optional<size_t> maxStringLength)
{
    const auto& extension = checkGetExtension(url, resource, mandatoryExtension);
    if (extension.has_value())
    {
        const auto& value = extension->valueString();
        ErpExpect(value.has_value(), HttpStatus::BadRequest,
                  "missing valueString.value in extension " + std::string(url));
        if (maxStringLength)
        {
            ErpExpectWithDiagnostics(String::utf8Length(*value) <= *maxStringLength, HttpStatus::BadRequest,
                                     "max length of " + std::to_string(*maxStringLength) +
                                         " exceeded by value in extension: " + std::string(url),
                                     std::string(*value));
        }
    }
}

void KbvValidationUtils::checkKbvExtensionValueCode(const std::string_view& url, const model::ResourceBase& resource,
                                                    bool mandatoryExtension, const std::set<std::string_view>& valueSet)
{
    const auto& extension = checkGetExtension(url, resource, mandatoryExtension);
    if (extension.has_value())
    {
        const auto& code = extension->valueCode();
        ErpExpect(code.has_value(), HttpStatus::BadRequest,
                  "missing valueCoding.code in extension " + std::string(url));
        ErpExpectWithDiagnostics(valueSet.count(*code) > 0, HttpStatus::BadRequest,
                                 "value is not in the allowed set " + setToString(valueSet) + " in extension " +
                                     std::string(url),
                                 std::string(*code));
    }
}

void KbvValidationUtils::checkKbvExtensionValueDate(const std::string_view& url, const model::ResourceBase& resource,
                                                    bool mandatoryExtension)
{
    const auto& extension = checkGetExtension(url, resource, mandatoryExtension);
    if (extension.has_value())
    {
        const auto& date = extension->valueDate();
        ErpExpect(date.has_value(), HttpStatus::BadRequest, "missing valueDate in extension " + std::string(url));
    }
}

void KbvValidationUtils::checkKbvExtensionValueRatio(const std::string_view& url, const model::ResourceBase& resource,
                                                     bool mandatoryExtension)
{
    const auto& extension = checkGetExtension(url, resource, mandatoryExtension);
    if (extension.has_value())
    {
        const auto& numerator = extension->valueRatioNumerator();
        ErpExpect(numerator.has_value(), HttpStatus::BadRequest,
                  "missing valueRatio.numerator in extension " + std::string(url));
        const auto& denominator = extension->valueRatioDenominator();
        ErpExpect(denominator.has_value(), HttpStatus::BadRequest,
                  "missing valueRatio.denominator in extension " + std::string(url));
    }
}

void KbvValidationUtils::checkKbvExtensionValuePeriod(const std::string_view& url, const model::ResourceBase& resource,
                                                      bool mandatoryExtension, bool valueMandatory)
{
    const auto& extension = checkGetExtension(url, resource, mandatoryExtension);
    if (extension.has_value())
    {
        const auto& start = extension->valuePeriodStart();
        const auto& end = extension->valuePeriodEnd();
        if (valueMandatory)
        {
            // startOrEnd:Es ist mindestens ein Start- oder ein Enddatum anzugeben
            ErpExpect(start.has_value() || end.has_value(), HttpStatus::BadRequest,
                      "missing valuePeriod.start or valuePeriod.end in extension " + std::string(url));
        }
    }
}

void KbvValidationUtils::checkKbvExtensionValueIdentifier(const std::string_view& url,
                                                          const model::ResourceBase& resource, bool mandatoryExtension,
                                                          const std::string_view& systemExpected, size_t maxValueLength)
{
    const auto& extension = checkGetExtension(url, resource, mandatoryExtension);
    if (extension.has_value())
    {
        const auto& system = extension->valueIdentifierSystem();
        const auto& value = extension->valueIdentifierValue();
        ErpExpectWithDiagnostics(systemExpected == system, HttpStatus::BadRequest,
                                 "Wrong system in extension: " + std::string(url), std::string(system.value_or("")));
        ErpExpect(value, HttpStatus::BadRequest,
                  "missing mandatory valueIdentifier.value in extension " + std::string(url));
        ErpExpectWithDiagnostics(String::utf8Length(*value) <= maxValueLength, HttpStatus::BadRequest,
                                 "max length of " + std::to_string(maxValueLength) +
                                     " exceeded by value in extension: " + std::string(url),
                                 std::string(*value));
    }
}

bool KbvValidationUtils::isKostentraeger(const model::KbvCoverage& coverage, const std::set<std::string>& kostentraeger)
{
    const auto& codingTypeCode = coverage.typeCodingCode();
    return (kostentraeger.count(std::string(codingTypeCode)) > 0);
}

static void checkNumeric(const std::string_view& number, const std::string& constraintError)
{
    try
    {
        size_t pos = 0;
        std::stoi(std::string(number), &pos);
        ErpExpectWithDiagnostics(pos == number.size(), HttpStatus::BadRequest, constraintError, std::string(number));
    }
    catch (const std::invalid_argument& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, constraintError, ex.what());
    }
    catch (const std::out_of_range& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, constraintError, ex.what());
    }
}

void KbvValidationUtils::checkIknr(const std::string_view& iknr)
{
    // ik-1
    // matches('[0-9]{8,9}')
    const auto *const constraintError = "Eine IK muss 8- (ohne Prüfziffer) oder 9-stellig (mit Prüfziffer) sein";
    ErpExpectWithDiagnostics(iknr.size() == 8 || iknr.size() == 9, HttpStatus::BadRequest, constraintError,
                             std::string(iknr));
    checkNumeric(iknr, constraintError);
}

void KbvValidationUtils::checkZanr(const std::string_view& zanr)
{
    // zanr-2
    // matches('[0-9]{9}')
    const auto *const constraintError = "Die KZVAbrechnungsnummer muss 9-stellig numerisch sein";
    ErpExpectWithDiagnostics(zanr.size() == 9, HttpStatus::BadRequest, constraintError, std::string(zanr));
    checkNumeric(zanr, constraintError);
}

void KbvValidationUtils::checkKvnr(const std::string_view& kvnr)
{
    // kvid-1
    // matches('^[A-Z][0-9]{9}$')
    const auto *const constraintError =
        "Der unveränderliche Teil der KVID muss 10-stellig sein und mit einem Großbuchstaben anfangen";
    ErpExpectWithDiagnostics(kvnr.size() == 10, HttpStatus::BadRequest, constraintError, std::string(kvnr));
    ErpExpectWithDiagnostics(kvnr[0] >= 'A' && kvnr[0] <= 'Z', HttpStatus::BadRequest, constraintError,
                             std::string(kvnr));
    checkNumeric(kvnr.substr(1), constraintError);
}

void KbvValidationUtils::checkLanr(const std::string_view& lanr)
{
    // lanr-1: Eine LANR muss neunstellig numerisch sein
    //matches('[0-9]{9}')
    const auto *const constraintError = "Eine LANR muss neunstellig numerisch sein";
    ErpExpectWithDiagnostics(lanr.size() == 9, HttpStatus::BadRequest, constraintError, std::string(lanr));
    checkNumeric(lanr, constraintError);
}

void KbvValidationUtils::checkFamilyName(const std::optional<model::UnspecifiedResource>& familyName)
{
    if (familyName)
    {
        KbvValidationUtils::checkKbvExtensionValueString("http://fhir.de/StructureDefinition/humanname-namenszusatz",
                                                         *familyName, false, 20);
        KbvValidationUtils::checkKbvExtensionValueString("http://hl7.org/fhir/StructureDefinition/humanname-own-name",
                                                         *familyName, false, 45);
        KbvValidationUtils::checkKbvExtensionValueString("http://hl7.org/fhir/StructureDefinition/humanname-own-prefix",
                                                         *familyName, false, 20);
    }
}

void KbvValidationUtils::checkNamePrefix(const std::optional<model::UnspecifiedResource>& namePrefix)
{
    if (namePrefix)
    {
        KbvValidationUtils::checkKbvExtensionValueCode("http://hl7.org/fhir/StructureDefinition/iso21090-EN-qualifier",
                                                       *namePrefix, true, {"AC"});
    }
}

void KbvValidationUtils::hum_1_2_3(const std::optional<model::UnspecifiedResource>& _family,
                                   const std::optional<std::string_view>& family)
{
    // hum-1:Wenn die Extension 'namenszusatz' verwendet wird, dann muss der vollständige Name im Attribut 'family' angegeben werden
    // family.extension('http://fhir.de/StructureDefinition/humanname-namenszusatz').empty() or family.hasValue()
    //
    // hum-2:Wenn die Extension 'nachname' verwendet wird, dann muss der vollständige Name im Attribut 'family' angegeben werden
    // family.extension('http://hl7.org/fhir/StructureDefinition/humanname-own-name').empty() or family.hasValue()
    //
    // hum-3:Wenn die Extension 'vorsatzwort' verwendet wird, dann muss der vollständige Name im Attribut 'family' angegeben werden
    // family.extension('http://hl7.org/fhir/StructureDefinition/humanname-own-prefix').empty() or family.hasValue()
    if (_family)
    {
        const auto& extNamenszusatz =
            _family->getExtension<model::Extension>("http://fhir.de/StructureDefinition/humanname-namenszusatz");
        const auto& extOwnName =
            _family->getExtension<model::Extension>("http://hl7.org/fhir/StructureDefinition/humanname-own-name");
        const auto& extOwnPrefix =
            _family->getExtension<model::Extension>("http://hl7.org/fhir/StructureDefinition/humanname-own-prefix");
        ErpExpect(! extNamenszusatz || family, HttpStatus::BadRequest,
                  "Wenn die Extension 'namenszusatz' verwendet wird, dann muss der vollständige Name im Attribut "
                  "'family' angegeben werden");
        ErpExpect(! extOwnName || family, HttpStatus::BadRequest,
                  "Wenn die Extension 'nachname' verwendet wird, dann muss der vollständige Name im Attribut 'family' "
                  "angegeben werden");
        ErpExpect(! extOwnPrefix || family, HttpStatus::BadRequest,
                  "Wenn die Extension 'vorsatzwort' verwendet wird, dann muss der vollständige Name im Attribut "
                  "'family' angegeben werden");
    }
}

void KbvValidationUtils::hum_4(const std::optional<model::UnspecifiedResource>& _prefix,
                               const std::optional<std::string_view>& prefix)
{
    // hum-4:Wenn die Extension 'prefix-qualifier' verwendet wird, dann muss ein Namenspräfix im Attribut 'prefix' angegeben werden
    // prefix.all($this.extension('http://hl7.org/fhir/StructureDefinition/iso21090-EN-qualifier').empty() or $this.hasValue())
    if (_prefix)
    {
        const auto& ext =
            _prefix->getExtension<model::Extension>("http://hl7.org/fhir/StructureDefinition/iso21090-EN-qualifier");
        ErpExpect(! ext || prefix, HttpStatus::BadRequest,
                  "Wenn die Extension 'prefix-qualifier' verwendet wird, dann muss ein Namenspräfix im Attribut "
                  "'prefix' angegeben werden");
    }
}

void KbvValidationUtils::add1(const std::optional<std::string_view>& line,
                              const std::optional<model::UnspecifiedResource>& _line)
{
    // add-1:
    // line.all($this.extension('http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-houseNumber').empty()
    //      or $this.hasValue())
    if (_line)
    {
        const auto& ext =
            _line->getExtension<model::Extension>("http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-houseNumber");
        ErpExpect(! ext || line, HttpStatus::BadRequest, ::constraintError(KbvAddressLineConstraint::add_1));
    }
}

void KbvValidationUtils::add2(const std::optional<std::string_view>& line,
                              const std::optional<model::UnspecifiedResource>& _line)
{
    // add-2:
    // line.all($this.extension('http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-streetName').empty()
    //      or $this.hasValue())
    if (_line)
    {
        const auto& ext =
            _line->getExtension<model::Extension>("http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-streetName");
        ErpExpect(! ext || line, HttpStatus::BadRequest, ::constraintError(KbvAddressLineConstraint::add_2));
    }
}

void KbvValidationUtils::add3(const std::optional<std::string_view>& line,
                              const std::optional<model::UnspecifiedResource>& _line)
{
    // add-3:
    // line.all($this.extension('http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-postBox').empty()
    //      or $this.hasValue())
    if (_line)
    {
        const auto& ext =
            _line->getExtension<model::Extension>("http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-postBox");
        ErpExpect(! ext || line, HttpStatus::BadRequest, ::constraintError(KbvAddressLineConstraint::add_3));
    }
}
void KbvValidationUtils::add4(const std::optional<std::string_view>& line,
                              const std::optional<model::UnspecifiedResource>& _line, const std::string_view& type)
{
    // add-4:
    // line.all($this.extension('http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-postBox').empty()
    //      or $this.hasValue()) or type='postal' or type.empty()
    if (_line)
    {
        const auto& ext =
            _line->getExtension<model::Extension>("http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-postBox");
        ErpExpect((! ext || line) || type == "postal" || type.empty(), HttpStatus::BadRequest,
                  ::constraintError(KbvAddressLineConstraint::add_4));
    }
}
void KbvValidationUtils::add5(const std::optional<std::string_view>& line,
                              const std::optional<model::UnspecifiedResource>& _line)
{
    // add-5:
    // line.all($this.extension('http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-additionalLocator').empty()
    //      or $this.hasValue())
    if (_line)
    {
        const auto& ext = _line->getExtension<model::Extension>(
            "http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-additionalLocator");
        ErpExpect(! ext || line, HttpStatus::BadRequest, ::constraintError(KbvAddressLineConstraint::add_5));
    }
}
void KbvValidationUtils::add6(const std::optional<model::UnspecifiedResource>& _line)
{
    // add-6:
    // line.all($this.extension('http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-postBox').empty()
    //      or ($this.extension('http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-streetName').empty()
    //      and $this.extension('http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-houseNumber').empty()))
    if (_line)
    {
        const auto& extPostBox =
            _line->getExtension<model::Extension>("http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-postBox");
        const auto& extStreetName =
            _line->getExtension<model::Extension>("http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-streetName");
        const auto& extHouseNumber =
            _line->getExtension<model::Extension>("http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-houseNumber");
        ErpExpect(! extPostBox || (! extStreetName && ! extHouseNumber), HttpStatus::BadRequest,
                  ::constraintError(KbvAddressLineConstraint::add_6));
    }
}
void KbvValidationUtils::add7(const model::UnspecifiedResource& address, const std::optional<std::string_view>& line,
                              const std::optional<std::string_view>& line2)
{
    // add-7:
    // extension('http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-precinct').empty()
    //      or line.where(extension('http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-precinct').valueString=$this)
    //          .exists()
    const auto& extPrecinct =
        address.getExtension<model::Extension>("http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-precinct");
    if (extPrecinct)
    {
        const auto valueString = extPrecinct->valueString();
        ErpExpect(valueString, HttpStatus::BadRequest, ::constraintError(KbvAddressLineConstraint::add_7));
        ErpExpect(valueString == line || valueString == line2, HttpStatus::BadRequest,
                  ::constraintError(KbvAddressLineConstraint::add_7));
    }
}
