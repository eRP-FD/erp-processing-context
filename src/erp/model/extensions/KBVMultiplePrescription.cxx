/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/extensions/KBVMultiplePrescription.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/TLog.hxx"


using model::KBVMultiplePrescription;

bool model::KBVMultiplePrescription::isMultiplePrescription() const
{
    auto kennzeichen = getExtension<Kennzeichen>();
    std::optional<bool> res = kennzeichen ? kennzeichen->valueBoolean() : std::nullopt;
    return res && res.value();
}

std::optional<int> model::KBVMultiplePrescription::numerator() const
{
    auto nummerierung = getExtension<Nummerierung>();
    if (nummerierung)
    {
        auto num = nummerierung->valueRatioNumerator();
        if (num)
        {
            return static_cast<int>(num.value());
        }
    }
    return std::nullopt;
}
std::optional<int> model::KBVMultiplePrescription::denominator() const
{
    auto nummerierung = getExtension<Nummerierung>();
    if (nummerierung)
    {
        auto denom = nummerierung->valueRatioDenominator();
        if (denom)
        {
            return static_cast<int>(denom.value());
        }
    }
    return std::nullopt;
}
std::optional<model::Timestamp> model::KBVMultiplePrescription::startDate() const
{
    auto zeitraum = getExtension<Zeitraum>();
    return zeitraum ? zeitraum->valuePeriodStartGermanDate() : std::nullopt;
}
std::optional<model::Timestamp> model::KBVMultiplePrescription::endDate() const
{
    auto zeitraum = getExtension<Zeitraum>();
    return zeitraum ? zeitraum->valuePeriodEndGermanDate() : std::nullopt;
}
