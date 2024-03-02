/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Extension.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/Expect.hxx"

#include <rapidjson/pointer.h>

using model::Extension;

namespace
{
const rapidjson::Pointer valuePeriodStartPointer(
    model::resource::ElementName::path(model::resource::elements::valuePeriod, model::resource::elements::start));
const rapidjson::Pointer valuePeriodEndPointer(
    model::resource::ElementName::path(model::resource::elements::valuePeriod, model::resource::elements::end));
}

std::optional<bool> Extension::valueBoolean() const
{
    const auto* value = getValue(rapidjson::Pointer{resource::ElementName::path(resource::elements::valueBoolean)});
    if (! value)
    {
        return std::nullopt;
    }
    ModelExpect(value->IsBool(), std::string{resource::elements::valueBoolean} + " is not a bool.");
    return std::make_optional(value->GetBool());
}

std::optional<std::string_view> model::Extension::valueCode() const
{
    static const rapidjson::Pointer codePointer(resource::ElementName::path(resource::elements::valueCode));
    return getOptionalStringValue(codePointer);
}

std::optional<std::string_view> model::Extension::valueCodingSystem() const
{
    static const rapidjson::Pointer systemPointer(
        resource::ElementName::path(resource::elements::valueCoding, resource::elements::system));
    return getOptionalStringValue(systemPointer);
}

std::optional<std::string_view> model::Extension::valueCodingCode() const
{
    static const rapidjson::Pointer codePointer(
        resource::ElementName::path(resource::elements::valueCoding, resource::elements::code));
    return getOptionalStringValue(codePointer);
}

std::optional<std::string_view> model::Extension::valueCodingDisplay() const
{
    static const rapidjson::Pointer codePointer(
        resource::ElementName::path(resource::elements::valueCoding, resource::elements::display));
    return getOptionalStringValue(codePointer);
}

std::optional<std::string_view> model::Extension::valueString() const
{
    static const rapidjson::Pointer valuePointer(resource::ElementName::path(resource::elements::valueString));
    return getOptionalStringValue(valuePointer);
}

std::optional<model::Timestamp> model::Extension::valueDate() const
{
    static const rapidjson::Pointer valuePointer(resource::ElementName::path(resource::elements::valueDate));
    const auto strVal = getOptionalStringValue(valuePointer);
    if (strVal)
    {
        return model::Timestamp::fromFhirDateTime(std::string(*strVal));
    }
    return std::nullopt;
}

std::optional<double> model::Extension::valueRatioNumerator() const
{
    static const rapidjson::Pointer valuePointer(resource::ElementName::path(
        resource::elements::valueRatio, resource::elements::numerator, resource::elements::value));
    return getOptionalDoubleValue(valuePointer);
}

std::optional<double> model::Extension::valueRatioDenominator() const
{
    static const rapidjson::Pointer valuePointer(resource::ElementName::path(
        resource::elements::valueRatio, resource::elements::denominator, resource::elements::value));
    return getOptionalDoubleValue(valuePointer);
}

std::optional<model::Timestamp> model::Extension::valuePeriodStart(const std::string& fallbackTimezone) const
{
    const auto strVal = getOptionalStringValue(valuePeriodStartPointer);
    if (strVal)
    {
        return model::Timestamp::fromFhirDateTime(std::string(*strVal), fallbackTimezone);
    }
    return std::nullopt;
}

std::optional<model::Timestamp> model::Extension::valuePeriodEnd(const std::string& fallbackTimezone) const
{
    const auto strVal = getOptionalStringValue(valuePeriodEndPointer);
    if (strVal)
    {
        return model::Timestamp::fromFhirDateTime(std::string(*strVal), fallbackTimezone);
    }
    return std::nullopt;
}

std::optional<model::Timestamp> model::Extension::valuePeriodStartGermanDate() const
{
    const auto strVal = getOptionalStringValue(valuePeriodStartPointer);
    if (strVal)
    {
        return model::Timestamp::fromGermanDate(std::string(*strVal));
    }
    return std::nullopt;
}
std::optional<model::Timestamp> model::Extension::valuePeriodEndGermanDate() const
{
    const auto strVal = getOptionalStringValue(valuePeriodEndPointer);
    if (strVal)
    {
        return model::Timestamp::fromGermanDate(std::string(*strVal));
    }
    return std::nullopt;
}

std::optional<std::string_view> model::Extension::valueIdentifierSystem() const
{
    static const rapidjson::Pointer valuePointer(
        resource::ElementName::path(resource::elements::valueIdentifier, resource::elements::system));
    return getOptionalStringValue(valuePointer);
}

std::optional<std::string_view> model::Extension::valueIdentifierValue() const
{
    static const rapidjson::Pointer valuePointer(
        resource::ElementName::path(resource::elements::valueIdentifier, resource::elements::value));
    return getOptionalStringValue(valuePointer);
}

std::optional<std::string_view> model::Extension::valueIdentifierTypeSystem() const
{
    static const rapidjson::Pointer valueIdentifierTypeSystemPointer(
        resource::ElementName::path(resource::elements::valueIdentifier, resource::elements::type,
                                    resource::elements::coding, "0", resource::elements::system));
    return getOptionalStringValue(valueIdentifierTypeSystemPointer);
}

std::optional<std::string_view> model::Extension::valueIdentifierTypeCode() const
{
    static const rapidjson::Pointer valueIdentifierTypeCodePointer(
        resource::ElementName::path(resource::elements::valueIdentifier, resource::elements::type,
                                    resource::elements::coding, "0", resource::elements::code));
    return getOptionalStringValue(valueIdentifierTypeCodePointer);
}

std::optional<std::string_view> model::Extension::valueIdentifierUse() const
{
    static const rapidjson::Pointer valueIdentifierUsePointer(
        resource::ElementName::path(resource::elements::valueIdentifier, resource::elements::use));
    return getOptionalStringValue(valueIdentifierUsePointer);
}
