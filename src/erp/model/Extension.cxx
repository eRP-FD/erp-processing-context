/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Extension.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/model/extensions/ChargeItemMarkingFlags.hxx"
#include "erp/model/extensions/KBVEXERPAccident.hxx"
#include "erp/model/extensions/KBVEXFORLegalBasis.hxx"
#include "erp/model/extensions/KBVMedicationCategory.hxx"
#include "erp/model/extensions/KBVMultiplePrescription.hxx"
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

template<typename ExtensionT>
std::optional<bool> Extension<ExtensionT>::valueBoolean() const
{
    const auto* value = getValue(rapidjson::Pointer{resource::ElementName::path(resource::elements::valueBoolean)});
    if (! value)
    {
        return std::nullopt;
    }
    ModelExpect(value->IsBool(), std::string{resource::elements::valueBoolean} + " is not a bool.");
    return std::make_optional(value->GetBool());
}

template<typename ExtensionT>
std::optional<std::string_view> model::Extension<ExtensionT>::valueCode() const
{
    static const rapidjson::Pointer codePointer(resource::ElementName::path(resource::elements::valueCode));
    return getOptionalStringValue(codePointer);
}

template<typename ExtensionT>
std::optional<std::string_view> model::Extension<ExtensionT>::valueCodingSystem() const
{
    static const rapidjson::Pointer systemPointer(
        resource::ElementName::path(resource::elements::valueCoding, resource::elements::system));
    return getOptionalStringValue(systemPointer);
}

template<typename ExtensionT>
std::optional<std::string_view> model::Extension<ExtensionT>::valueCodingCode() const
{
    static const rapidjson::Pointer codePointer(
        resource::ElementName::path(resource::elements::valueCoding, resource::elements::code));
    return getOptionalStringValue(codePointer);
}

template<typename ExtensionT>
std::optional<std::string_view> model::Extension<ExtensionT>::valueCodingDisplay() const
{
    static const rapidjson::Pointer codePointer(
        resource::ElementName::path(resource::elements::valueCoding, resource::elements::display));
    return getOptionalStringValue(codePointer);
}

template<typename ExtensionT>
std::optional<std::string_view> model::Extension<ExtensionT>::valueString() const
{
    static const rapidjson::Pointer valuePointer(resource::ElementName::path(resource::elements::valueString));
    return getOptionalStringValue(valuePointer);
}

template<typename ExtensionT>
std::optional<model::Timestamp> model::Extension<ExtensionT>::valueDate() const
{
    static const rapidjson::Pointer valuePointer(resource::ElementName::path(resource::elements::valueDate));
    const auto strVal = getOptionalStringValue(valuePointer);
    if (strVal)
    {
        return model::Timestamp::fromFhirDateTime(std::string(*strVal));
    }
    return std::nullopt;
}

template<typename ExtensionT>
std::optional<double> model::Extension<ExtensionT>::valueRatioNumerator() const
{
    static const rapidjson::Pointer valuePointer(resource::ElementName::path(
        resource::elements::valueRatio, resource::elements::numerator, resource::elements::value));
    return getOptionalDoubleValue(valuePointer);
}

template<typename ExtensionT>
std::optional<double> model::Extension<ExtensionT>::valueRatioDenominator() const
{
    static const rapidjson::Pointer valuePointer(resource::ElementName::path(
        resource::elements::valueRatio, resource::elements::denominator, resource::elements::value));
    return getOptionalDoubleValue(valuePointer);
}

template<typename ExtensionT>
std::optional<model::Timestamp>
model::Extension<ExtensionT>::valuePeriodStart(const std::string& fallbackTimezone) const
{
    const auto strVal = getOptionalStringValue(valuePeriodStartPointer);
    if (strVal)
    {
        return model::Timestamp::fromFhirDateTime(std::string(*strVal), fallbackTimezone);
    }
    return std::nullopt;
}

template<typename ExtensionT>
std::optional<model::Timestamp> model::Extension<ExtensionT>::valuePeriodEnd(const std::string& fallbackTimezone) const
{
    const auto strVal = getOptionalStringValue(valuePeriodEndPointer);
    if (strVal)
    {
        return model::Timestamp::fromFhirDateTime(std::string(*strVal), fallbackTimezone);
    }
    return std::nullopt;
}

template<typename ExtensionT>
std::optional<model::Timestamp> model::Extension<ExtensionT>::valuePeriodStartGermanDate() const
{
    const auto strVal = getOptionalStringValue(valuePeriodStartPointer);
    if (strVal)
    {
        return model::Timestamp::fromGermanDate(std::string(*strVal));
    }
    return std::nullopt;
}

template<typename ExtensionT>
std::optional<model::Timestamp> model::Extension<ExtensionT>::valuePeriodEndGermanDate() const
{
    const auto strVal = getOptionalStringValue(valuePeriodEndPointer);
    if (strVal)
    {
        return model::Timestamp::fromGermanDate(std::string(*strVal));
    }
    return std::nullopt;
}

template<typename ExtensionT>
std::optional<std::string_view> model::Extension<ExtensionT>::valueIdentifierSystem() const
{
    static const rapidjson::Pointer valuePointer(
        resource::ElementName::path(resource::elements::valueIdentifier, resource::elements::system));
    return getOptionalStringValue(valuePointer);
}

template<typename ExtensionT>
std::optional<std::string_view> model::Extension<ExtensionT>::valueIdentifierValue() const
{
    static const rapidjson::Pointer valuePointer(
        resource::ElementName::path(resource::elements::valueIdentifier, resource::elements::value));
    return getOptionalStringValue(valuePointer);
}

template<typename ExtensionT>
std::optional<std::string_view> model::Extension<ExtensionT>::valueIdentifierTypeSystem() const
{
    static const rapidjson::Pointer valueIdentifierTypeSystemPointer(
        resource::ElementName::path(resource::elements::valueIdentifier, resource::elements::type,
                                    resource::elements::coding, "0", resource::elements::system));
    return getOptionalStringValue(valueIdentifierTypeSystemPointer);
}

template<typename ExtensionT>
std::optional<std::string_view> model::Extension<ExtensionT>::valueIdentifierTypeCode() const
{
    static const rapidjson::Pointer valueIdentifierTypeCodePointer(
        resource::ElementName::path(resource::elements::valueIdentifier, resource::elements::type,
                                    resource::elements::coding, "0", resource::elements::code));
    return getOptionalStringValue(valueIdentifierTypeCodePointer);
}

template<typename ExtensionT>
std::optional<std::string_view> model::Extension<ExtensionT>::valueIdentifierUse() const
{
    static const rapidjson::Pointer valueIdentifierUsePointer(
        resource::ElementName::path(resource::elements::valueIdentifier, resource::elements::use));
    return getOptionalStringValue(valueIdentifierUsePointer);
}

template class model::Extension<model::ChargeItemMarkingFlags>;
template class model::Extension<model::ChargeItemMarkingFlag>;
template class model::Extension<model::KBVEXERPAccident>;
template class model::Extension<model::KBVEXFORLegalBasis>;
template class model::Extension<model::KBVMedicationCategory>;
template class model::Extension<model::KBVMultiplePrescription>;
template class model::Extension<model::KBVMultiplePrescription::Kennzeichen>;
template class model::Extension<model::KBVMultiplePrescription::Nummerierung>;
template class model::Extension<model::KBVMultiplePrescription::Zeitraum>;
template class model::Extension<model::KBVMultiplePrescription::ID>;
template class model::Extension<model::UnspecifiedExtension>;
