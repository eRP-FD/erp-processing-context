/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "fhirtools/model/DecimalType.hxx"
#include "fhirtools/model/DateTime.hxx"

namespace fhirtools
{
class QuantityType
{
public:
    QuantityType(DecimalType value, const std::string_view& unit);
    explicit QuantityType(const std::string_view& valueAndUnit);

    [[nodiscard]] const DecimalType& value() const;
    [[nodiscard]] const std::string& unit() const;
    [[nodiscard]] std::optional<std::strong_ordering> compareTo(const QuantityType& rhs) const;
    [[nodiscard]] std::optional<bool> equals(const QuantityType& rhs) const;

private:
    QuantityType convertToUnit(const std::string& unit) const;
    bool convertibleToUnit(const std::string& unit) const;

    DecimalType mValue;
    std::string mUnit;
};

std::string to_string(const QuantityType& q);

using PrimitiveValueVariant = std::variant<int32_t, DecimalType, bool, std::string, Date, DateTime, Time, QuantityType>;

class PrimitiveValue : public PrimitiveValueVariant
{
public:
    using PrimitiveValueVariant::PrimitiveValueVariant;
    // http://hl7.org/fhirpath/N1/#integer-conversion-functions
    [[nodiscard]] int32_t asInt() const;
    // http://hl7.org/fhirpath/N1/#decimal-conversion-functions
    [[nodiscard]] DecimalType asDecimal() const;
    // http://hl7.org/fhirpath/N1/#boolean-conversion-functions
    [[nodiscard]] bool asBool() const;
    // http://hl7.org/fhirpath/N1/#toquantityunit-string-quantity
    [[nodiscard]] std::string asString() const;
    // http://hl7.org/fhirpath/N1/#todate-date
    [[nodiscard]] Date asDate() const;
    // http://hl7.org/fhirpath/N1/#time-conversion-functions
    [[nodiscard]] Time asTime() const;
    // http://hl7.org/fhirpath/N1/#datetime-conversion-functions
    [[nodiscard]] DateTime asDateTime() const;
    // http://hl7.org/fhirpath/N1/#toquantityunit-string-quantity
    [[nodiscard]] QuantityType asQuantity() const;
};

}
