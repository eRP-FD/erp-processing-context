/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "shared/util/dosagetext/renderer/FrequencyPeriodRenderer.hxx"
#include "Localization.hxx"
#include "shared/model/DosageDgMP.hxx"

namespace dosagetext
{

void FrequencyPeriodRenderer::doRender(const std::vector<model::DosageDgMP>& dosages)
{
    if (!isFrequencyPattern(dosages))
    {
        return;
    }
    // _generate_frequency_description in the reference script
    // https://github.com/hl7germany/dgMP-DosageTextgenerierung-Skript/blob/a9683825be784db107adbd4b5cd796ffd4764394/medication-dosage-to-text.py#L687-L686
    // # For interval schema, use the first (and typically only) dosage
    const auto timing = dosages.at(0).timing();
    if (! timing)
    {
        return;
    }

    if (timing->timesOfDay.empty() && timing->when.empty() && timing->frequency != "1")
    {
        mText += timing->frequency + " x ";
    }
    if (timing->period == "1")
    {
        // special cases daily and weekly pattern:
        mText += germanFrequency(timing->periodUnit);
    }
    else
    {
        // generic pattern:
        // {frequency} x alle {period} {Einheit} (z. B. 3 x alle 8 Stunden)
        mText.append("alle ")
            .append(timing->period)
            .append(" ")
            .append(germanTimeUnit(timing->periodUnit, GrammaticalNumber::PLURAL));
    }
}

std::string FrequencyPeriodRenderer::text() const
{
    return mText;
}

std::string FrequencyPeriodRenderer::separator() const
{
    return ": ";
}

}
