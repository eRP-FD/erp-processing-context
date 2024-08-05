/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvPractitioner.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Expect.hxx"


namespace model
{

namespace
{
const rapidjson::Pointer identifierArrayPointer("/identifier");
const rapidjson::Pointer systemRelPointer("/system");
const rapidjson::Pointer valueRelPointer("/value");
}// namespace

KbvPractitioner::KbvPractitioner(NumberAsStringParserDocument&& document)
    : Resource<KbvPractitioner>(std::move(document))
{
}

std::string_view KbvPractitioner::qualificationTypeCode() const
{
    static const rapidjson::Pointer qualificationArrayPointer("/qualification");
    //https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Qualification_Type
    static const rapidjson::Pointer systemPointer("/code/coding/0/system");
    static const rapidjson::Pointer codePointer("/code/coding/0/code");
    const auto codeOpt = findStringInArray(qualificationArrayPointer, systemPointer,
                                           "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Qualification_Type", codePointer);
    ModelExpect(codeOpt.has_value(), "mandatory qualification code is missing");
    return codeOpt.value();
}

std::optional<UnspecifiedResource> KbvPractitioner::name_family() const
{
    static const rapidjson::Pointer nameFamilyPointer(
        resource::ElementName::path(resource::elements::name, "0", resource::elements::_family));
    return extractUnspecifiedResource(nameFamilyPointer);
}
std::optional<UnspecifiedResource> KbvPractitioner::name_prefix() const
{
    static const rapidjson::Pointer namePrefixPointer(
        resource::ElementName::path(resource::elements::name, "0", resource::elements::_prefix, "0"));
    return extractUnspecifiedResource(namePrefixPointer);
}
std::optional<std::string_view> KbvPractitioner::nameFamily() const
{
    static const rapidjson::Pointer nameFamilyPointer(
        resource::ElementName::path(resource::elements::name, "0", resource::elements::family));
    return getOptionalStringValue(nameFamilyPointer);
}
std::optional<std::string_view> KbvPractitioner::namePrefix() const
{
    static const rapidjson::Pointer namePrefixPointer(
        resource::ElementName::path(resource::elements::name, "0", resource::elements::prefix, "0"));
    return getOptionalStringValue(namePrefixPointer);
}

std::optional<std::string_view> KbvPractitioner::qualificationText(size_t idx) const
{
    const rapidjson::Pointer qualificationTextPointer(resource::ElementName::path(
        resource::elements::qualification, idx, resource::elements::code, resource::elements::text));
    return getOptionalStringValue(qualificationTextPointer);
}

std::optional<std::string_view> KbvPractitioner::qualificationCodeCodingSystem(size_t idx) const
{
    const rapidjson::Pointer qualificationCodeCodingSystemPointer(
        resource::ElementName::path(resource::elements::qualification, idx, resource::elements::code,
                                    resource::elements::coding, 0, resource::elements::system));
    return getOptionalStringValue(qualificationCodeCodingSystemPointer);
}

std::optional<std::string_view> KbvPractitioner::qualificationCodeCodingCode(size_t idx) const
{
    const rapidjson::Pointer qualificationCodeCodingSystemPointer(
        resource::ElementName::path(resource::elements::qualification, idx, resource::elements::code,
                                    resource::elements::coding, 0, resource::elements::code));
    return getOptionalStringValue(qualificationCodeCodingSystemPointer);
}

std::optional<Lanr> KbvPractitioner::anr() const
{
    auto anrString =
        findStringInArray(identifierArrayPointer, systemRelPointer, resource::naming_system::kbvAnr, valueRelPointer);
    if (! anrString)
    {
        return std::nullopt;
    }
    return Lanr{*anrString, Lanr::Type::lanr};
}

std::optional<Lanr> KbvPractitioner::zanr() const
{
    auto zanrString =
        findStringInArray(identifierArrayPointer, systemRelPointer, resource::naming_system::kbvZanr, valueRelPointer);
    if (! zanrString)
    {
        return std::nullopt;
    }
    return Lanr{*zanrString, Lanr::Type::zanr};
}

} // namespace model
