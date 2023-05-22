/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_EXTENSION_H
#define ERP_PROCESSING_CONTEXT_MODEL_EXTENSION_H

#include "erp/model/Resource.hxx"

#include <optional>

namespace model
{
class Timestamp;

// NOLINTNEXTLINE(bugprone-exception-escape)
class Extension : public model::Resource<Extension>
{
public:
    static constexpr auto resourceTypeName = "Extension";

    using Resource::Resource;
    [[nodiscard]] std::optional<bool> valueBoolean() const;

    [[nodiscard]] std::optional<std::string_view> valueCode() const;

    [[nodiscard]] std::optional<std::string_view> valueCodingSystem() const;
    [[nodiscard]] std::optional<std::string_view> valueCodingCode() const;
    [[nodiscard]] std::optional<std::string_view> valueCodingDisplay() const;

    [[nodiscard]] std::optional<std::string_view> valueString() const;

    [[nodiscard]] std::optional<model::Timestamp> valueDate() const;

    [[nodiscard]] std::optional<double> valueRatioNumerator() const;
    [[nodiscard]] std::optional<double> valueRatioDenominator() const;

    [[nodiscard]] std::optional<model::Timestamp> valuePeriodStart(const std::string& fallbackTimezone) const;
    [[nodiscard]] std::optional<model::Timestamp> valuePeriodEnd(const std::string& fallbackTimezone) const;
    [[nodiscard]] std::optional<model::Timestamp> valuePeriodStartGermanDate() const;
    [[nodiscard]] std::optional<model::Timestamp> valuePeriodEndGermanDate() const;


    [[nodiscard]] std::optional<std::string_view> valueIdentifierSystem() const;
    [[nodiscard]] std::optional<std::string_view> valueIdentifierValue() const;
    [[nodiscard]] std::optional<std::string_view> valueIdentifierTypeSystem() const;
    [[nodiscard]] std::optional<std::string_view> valueIdentifierTypeCode() const;
    [[nodiscard]] std::optional<std::string_view> valueIdentifierUse() const;

    friend std::optional<Extension> ResourceBase::getExtension<Extension>(const std::string_view&) const;
};

}


#endif// ERP_PROCESSING_CONTEXT_MODEL_EXTENSION_H
