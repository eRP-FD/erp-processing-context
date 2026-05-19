/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "shared/util/dosagetext/renderer/Renderer.hxx"
#include "shared/model/DosageDgMP.hxx"

#include <utility>

namespace dosagetext
{

Renderer::Renderer(ScriptVersion version, Language language)
    : mVersion(std::move(version))
    , mLanguage(std::move(language))
{
}

void Renderer::render(const std::vector<model::DosageDgMP>& dosages)
{
    if (! dosages.empty())
    {
        doRender(dosages);
    }
}

std::string Renderer::separator() const
{
    return " ";
}

const ScriptVersion& Renderer::scriptVersion() const
{
    return mVersion;
}

const Language& Renderer::language() const
{
    return mLanguage;
}

bool Renderer::is4Pattern(const std::vector<model::DosageDgMP>& dosages)
{
    if (dosages.empty())
    {
        return false;
    }
    const auto timing = dosages.front().timing();
    if (! timing)
    {
        return false;
    }
    return (isDaily(timing->periodUnit) || isWeekly(timing->periodUnit)) && timing->period == "1" &&
           ! timing->when.empty() && timing->timesOfDay.empty();
}

bool Renderer::isFrequencyPattern(const std::vector<model::DosageDgMP>& dosages)
{
    switch (detectSchema(dosages))
    {
        case Schema::UNKNOWN:
        case Schema::FREE_TEXT:
        case Schema::FOUR_PATTERN:
        case Schema::DAY_OF_WEEK:
        case Schema::DAY_TIME_COMBO:
            break;
        case Schema::TIME_OF_DAY:
        case Schema::INTERVAL:
        case Schema::INTERVAL_TIME_COMBO:
            return true;
    }
    return false;
}

Schema Renderer::detectSchema(const std::vector<model::DosageDgMP>& dosages)
{
    // https://github.com/hl7germany/dgMP-DosageTextgenerierung-Skript/blob/a9683825be784db107adbd4b5cd796ffd4764394/medication-dosage-to-text.py#L177-L176
    if (dosages.empty())
    {
        return Schema::UNKNOWN;
    }
    const auto& firstDosage = dosages.front();
    const auto& timing = firstDosage.timing();

    //# Schema 1: FreeText - has text but no structured timing
    if (firstDosage.text().has_value() && ! timing.has_value())
    {
        return Schema::FREE_TEXT;
    }
    if (! timing.has_value())
    {
        return Schema::UNKNOWN;
    }
    const bool hasDaysOfWeek = ! timing->daysOfWeek.empty();
    const bool hasWhen = ! timing->when.empty();
    const bool hasTimesOfDay = ! timing->timesOfDay.empty();
    const bool isDailyPattern = isDaily(timing->periodUnit) && timing->period == "1";

    //# Schema 2: 4-Schema - daily frequency with 'when' codes only
    if (isDailyPattern && hasWhen && ! hasDaysOfWeek && ! hasTimesOfDay)
    {
        return Schema::FOUR_PATTERN;
    }

    //# Schema 3: DayOfWeek - specific weekdays, daily period, no timing details
    if (hasDaysOfWeek && ! hasWhen && ! hasTimesOfDay)
    {
        return Schema::DAY_OF_WEEK;
    }

    //# Schema 4: DayOfWeek + Time/4-Schema - weekdays plus timing
    if (hasDaysOfWeek && (hasWhen || hasTimesOfDay))
    {
        return Schema::DAY_TIME_COMBO;
    }

    //# Schema 5: TimeOfDay - daily period with specific times only
    if (isDailyPattern && ! hasDaysOfWeek && hasTimesOfDay && ! hasWhen)
    {
        return Schema::TIME_OF_DAY;
    }

    //# Schema 6: Interval + Time/4-Schema - non-daily period with timing
    if (! hasDaysOfWeek && (hasWhen || hasTimesOfDay) && ! isDailyPattern)
    {
        return Schema::INTERVAL_TIME_COMBO;
    }

    //# Schema 7: Interval - pure interval without timing details
    if (! hasWhen && ! hasTimesOfDay && ! hasDaysOfWeek)
    {
        return Schema::INTERVAL;
    }

    return Schema::UNKNOWN;
}

}
