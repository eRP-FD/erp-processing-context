/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/extensions/KBVMultiplePrescription.hxx"
#include "shared/model/Extension.txx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/TLog.hxx"


using model::KBVMultiplePrescription;

bool model::KBVMultiplePrescription::isMultiplePrescription() const
{
    auto kennzeichen = getExtension<Kennzeichen>();
    std::optional<bool> res = kennzeichen ? kennzeichen->valueBoolean() : std::nullopt;
    return res && res.value();
}

std::optional<std::string> model::KBVMultiplePrescription::mvoId() const
{
    auto id = getExtension<ID>();
    return id ? std::make_optional<std::string>(*id->valueIdentifierValue()) : std::nullopt;
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

std::optional<model::Timestamp> model::KBVMultiplePrescription::startDateTime() const
{
    auto zeitraum = getExtension<Zeitraum>();
    return zeitraum ? zeitraum->valuePeriodStart(Timestamp::GermanTimezone) : std::nullopt;
}

std::optional<model::Timestamp> model::KBVMultiplePrescription::endDateTime() const
{
    auto zeitraum = getExtension<Zeitraum>();
    return zeitraum ? zeitraum->valuePeriodEnd(Timestamp::GermanTimezone) : std::nullopt;
}

template class model::Extension<KBVMultiplePrescription>;
template class model::Extension<KBVMultiplePrescription::Kennzeichen>;
template class model::Extension<KBVMultiplePrescription::Nummerierung>;
template class model::Extension<KBVMultiplePrescription::Zeitraum>;
template class model::Extension<KBVMultiplePrescription::ID>;
