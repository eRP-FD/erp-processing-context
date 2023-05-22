/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvCoverage.hxx"
#include "erp/model/ResourceNames.hxx"

namespace model
{

KbvCoverage::KbvCoverage(NumberAsStringParserDocument&& jsonTree)
    : Resource(std::move(jsonTree))
{
}

std::string_view KbvCoverage::typeCodingCode() const
{
    static const rapidjson::Pointer pointer(resource::ElementName::path(
        resource::elements::type, resource::elements::coding, "0", resource::elements::code));
    return getStringValue(pointer);
}

bool KbvCoverage::hasPayorIdentifierExtension(const std::string_view& url) const
{
    static const rapidjson::Pointer extensionArrayPointer(resource::ElementName::path(
        resource::elements::payor, "0", resource::elements::identifier, resource::elements::extension));
    static const rapidjson::Pointer urlPointer(resource::ElementName::path(resource::elements::url));
    static const rapidjson::Pointer valuePointer(
        resource::ElementName::path(resource::elements::valueIdentifier, resource::elements::value));
    const auto alternativeId = findStringInArray(extensionArrayPointer, urlPointer, url, valuePointer);
    return alternativeId.has_value();
}

std::optional<std::string_view> KbvCoverage::payorIdentifierValue() const
{
    static const rapidjson::Pointer payorIdentifierValuePointer(resource::ElementName::path(
        resource::elements::payor, 0, resource::elements::identifier, resource::elements::value));
    return getOptionalStringValue(payorIdentifierValuePointer);
}

std::optional<std::string_view> KbvCoverage::periodEnd() const
{
    static const rapidjson::Pointer periodEndPointer(resource::ElementName::path(
        resource::elements::period, resource::elements::end));
    return getOptionalStringValue(periodEndPointer);
}

const rapidjson::Value* KbvCoverage::payorIdentifier() const
{
    static const rapidjson::Pointer payorIdentifierPointer(resource::ElementName::path(
        resource::elements::payor, 0, resource::elements::identifier));
    return getValue(payorIdentifierPointer);
}

}
