/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/KbvCoverage.hxx"
#include "shared/model/ResourceNames.hxx"

namespace model
{

namespace
{
const rapidjson::Pointer extensionArrayPointer(resource::ElementName::path(resource::elements::payor, "0",
                                                                           resource::elements::identifier,
                                                                           resource::elements::extension));
const rapidjson::Pointer urlPointer(resource::ElementName::path(resource::elements::url));
const rapidjson::Pointer valuePointer(resource::ElementName::path(resource::elements::valueIdentifier,
                                                                  resource::elements::value));
}// namespace

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
    const auto alternativeId = findStringInArray(extensionArrayPointer, urlPointer, url, valuePointer);
    return alternativeId.has_value();
}

std::optional<Iknr> KbvCoverage::payorIknr() const
{
    static const rapidjson::Pointer payorIdentifierValuePointer(resource::ElementName::path(
        resource::elements::payor, 0, resource::elements::identifier, resource::elements::value));
    auto iknr = getOptionalStringValue(payorIdentifierValuePointer);
    if (! iknr)
    {
        return std::nullopt;
    }
    return model::Iknr(*iknr);
}

std::optional<Iknr> KbvCoverage::alternativeId() const
{
    const auto alternativeId =
        findStringInArray(extensionArrayPointer, urlPointer, resource::extension::alternativeIk, valuePointer);
    if (! alternativeId)
    {
        return std::nullopt;
    }
    return model::Iknr(*alternativeId);
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

} // namespace model
