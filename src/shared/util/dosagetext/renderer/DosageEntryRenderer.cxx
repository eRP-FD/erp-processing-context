/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/dosagetext/renderer/DosageEntryRenderer.hxx"
#include "shared/model/DosageDgMP.hxx"
#include "shared/util/dosagetext/renderer/Localization.hxx"

#include <map>
#include <utility>

namespace dosagetext
{

DosageEntryRenderer::DosageEntryRenderer(ScriptVersion version, Language language)
    : Renderer(std::move(version), std::move(language))
{
}

std::string DosageEntryRenderer::text() const
{
    return mText;
}

std::string DosageEntryRenderer::separator() const
{
    return "\n";
}

struct DosageEntryRenderer::QuantityPerTime {
    std::optional<std::string> timeOfDay;
    std::optional<model::EventTiming> eventTiming;
    auto operator<=>(const QuantityPerTime&) const = default;
};

struct DosageEntryRenderer::QuantitiesPerTime {
    void insert(model::SimpleQuantity inQuantity, std::optional<std::string> timeOfDay,
                std::optional<model::EventTiming> eventTiming)
    {
        originalSortOrder.emplace_back(QuantityPerTime{.timeOfDay = std::move(timeOfDay), .eventTiming = eventTiming});
        timeToQuantity.emplace(originalSortOrder.back(), std::move(inQuantity));
    }
    void merge(const QuantitiesPerTime& other)
    {
        for (const auto& perTime : other.originalSortOrder)
        {
            const auto& quantity = other.timeToQuantity.at(perTime);
            insert(quantity, perTime.timeOfDay, perTime.eventTiming);
        }
    }

    std::map<QuantityPerTime, model::SimpleQuantity> timeToQuantity;
    std::vector<QuantityPerTime> originalSortOrder;
};

void DosageEntryRenderer::doRender(const std::vector<model::DosageDgMP>& dosages)
{
    std::map<std::optional<model::DaysOfWeek>, QuantitiesPerTime> daysOfWeekQuantities;

    for (const auto& dosage : dosages)
    {
        const auto& timing = dosage.timing();
        auto doseQuantity = dosage.doseQuantity();
        if (! doseQuantity || ! timing)
        {
            continue;
        }

        QuantitiesPerTime quantitiesPerTime;
        for (const auto& timeOfDay : timing->timesOfDay)
        {
            quantitiesPerTime.insert(*doseQuantity, timeOfDay, std::nullopt);
        }
        for (const auto& when : timing->when)
        {
            quantitiesPerTime.insert(*doseQuantity, std::nullopt, when);
        }
        if (quantitiesPerTime.timeToQuantity.empty())
        {
            quantitiesPerTime.insert(*doseQuantity, std::nullopt, std::nullopt);
        }
        if (! timing->daysOfWeek.empty())
        {
            for (const auto& dayOfWeek : timing->daysOfWeek)
            {
                daysOfWeekQuantities[dayOfWeek].merge(quantitiesPerTime);
            }
        }
        else
        {
            daysOfWeekQuantities[std::nullopt].merge(quantitiesPerTime);
        }
    }

    if (is4Pattern(dosages))
    {
        // Kompakte 4‑Schema Notation (nur when, tägliches Muster)
        mText = render4Pattern(daysOfWeekQuantities);
    }
    else
    {
        mText = renderLinePattern(daysOfWeekQuantities, shouldMergeTimes(dosages), replicateTimesSortBug(dosages));
    }
}

std::string DosageEntryRenderer::render4Pattern(
    const std::map<std::optional<model::DaysOfWeek>, QuantitiesPerTime>& daysOfWeekQuanities)
{
    std::string whenString;
    for (const auto& [dayOfWeek, quantitiesPerTime] : daysOfWeekQuanities)
    {
        if (! whenString.empty())
        {
            whenString.append("; ");
        }
        if (dayOfWeek.has_value())
        {
            whenString.append(germanWeekdayAdverb(*dayOfWeek)).append(" ");
        }
        // <MORN>-<NOON>-<EVE>-<NIGHT> <Einheit>
        std::string_view sep;
        std::string_view unit;
        for (auto event_timing : magic_enum::enum_values<model::EventTiming>())
        {
            whenString.append(sep);
            sep = "-";
            const QuantityPerTime quantityPerTime{.timeOfDay = std::nullopt, .eventTiming = event_timing};
            if (quantitiesPerTime.timeToQuantity.contains(quantityPerTime))
            {
                whenString.append(quantitiesPerTime.timeToQuantity.at(quantityPerTime).value);
                if (unit.empty())
                {
                    unit = quantitiesPerTime.timeToQuantity.at(quantityPerTime).unit;
                }
            }
            else
            {
                whenString.append("0");
            }
        }
        if (! unit.empty())
        {
            whenString.append(" ").append(unit);
        }
    }
    return whenString;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::string DosageEntryRenderer::renderLinePattern(
    const std::map<std::optional<model::DaysOfWeek>, QuantitiesPerTime>& daysOfWeekQuanities, bool shouldMergeTimes,
    bool shouldReplicateTimesSortBug)
{
    std::string text;
    for (const auto& [dayOfWeek, quantitiesPerTime] : daysOfWeekQuanities)
    {
        if (! text.empty())
        {
            text.append("; ");
        }
        std::string_view sepHyphen;
        if (dayOfWeek.has_value())
        {
            text.append(germanWeekdayAdverb(*dayOfWeek)).append(" ");
            sepHyphen = "— ";
        }
        std::string_view sepLine;
        std::optional<model::SimpleQuantity> lastQuantity;
        auto sortOrder = quantitiesPerTime.originalSortOrder;
        if (! shouldReplicateTimesSortBug)
        {
            // Note times in the same dosage are already sorted. this here additionally sorts times from different dosages
            // src/shared/model/DosageDgMP.cxx:122
            std::ranges::sort(sortOrder);
        }
        for (const auto& perTime : sortOrder)
        {
            const auto& quantity = quantitiesPerTime.timeToQuantity.at(perTime);
            // merge times with the same quantity
            if (lastQuantity.has_value() && (quantity != *lastQuantity || ! shouldMergeTimes))
            {
                text.append(sepHyphen).append("je ").append(lastQuantity->value).append(" ").append(lastQuantity->unit);
                sepLine = "; ";
                sepHyphen = "";
            }
            text.append(sepLine);
            sepLine = ", ";
            lastQuantity = quantity;
            if (perTime.eventTiming.has_value())
            {
                text.append(germanEventTiming(*perTime.eventTiming));
                sepHyphen = " — ";
            }
            if (perTime.timeOfDay.has_value())
            {
                text.append(germanTimeOfDay(*perTime.timeOfDay));
                sepHyphen = " — ";
            }
        }
        if (lastQuantity.has_value())
        {
            text.append(sepHyphen).append("je ").append(lastQuantity->value).append(" ").append(lastQuantity->unit);
        }
    }
    return text;
}

bool DosageEntryRenderer::shouldMergeTimes(const std::vector<model::DosageDgMP>& dosages)
{
    // The reference script merges times like
    // "täglich: 08:00 Uhr, 14:00 Uhr, 22:00 Uhr — je 1 Stück"
    // "montags 09:00 Uhr, 21:00 Uhr — je 1 Stück; mittwochs 09:00 Uhr, 21:00 Uhr — je 1 Stück; freitags 09:00 Uhr, 21:00 Uhr — je 1 Stück"
    // "für 10 Tage täglich: 08:00 Uhr — je 2 Stück; 11:00 Uhr, 14:00 Uhr, 17:00 Uhr, 20:00 Uhr, 23:00 Uhr — je 1 Stück"
    // "montags 08:00 Uhr, 20:00 Uhr — je 2 Stück; dienstags 08:00 Uhr — je 1 Stück; donnerstags 08:00 Uhr — je 1 Stück; freitags 08:00 Uhr, 20:00 Uhr — je 2 Stück"
    // but not:
    // "alle 2 Tage: 08:00 Uhr — je 1 Stück; 20:00 Uhr — je 1 Stück"
    // "alle 3 Tage: morgens — je 1 Stück; abends — je 1 Stück"
    // "montags 1-0-1-0 Stück; freitags 1-0-1-0 Stück"
    // "alle 2 Tage: 08:00 Uhr — je 1 Stück; 10:00 Uhr — je 2 Stück; 14:00 Uhr — je 2 Stück; 20:00 Uhr — je 1 Stück; 22:00 Uhr — je 2 Stück"
    switch (detectSchema(dosages))
    {
        case Schema::UNKNOWN:
        case Schema::FREE_TEXT:
        case Schema::FOUR_PATTERN:
        case Schema::INTERVAL_TIME_COMBO:
            break;
        case Schema::TIME_OF_DAY:
        case Schema::DAY_OF_WEEK:
        case Schema::INTERVAL:
        case Schema::DAY_TIME_COMBO:
            return true;
    }
    return false;
}

bool DosageEntryRenderer::replicateTimesSortBug(const std::vector<model::DosageDgMP>& dosages)
{
    // The 1.0.1 script does not sort times for Schema 5: TimeOfDay, although it should according to specification
    // https://ig.fhir.de/igs/medication/dosierung-textgenerierung.html#4-konkrete-zeiten-timeofday
    switch (detectSchema(dosages))
    {
        case Schema::UNKNOWN:
        case Schema::FREE_TEXT:
        case Schema::FOUR_PATTERN:
        case Schema::INTERVAL_TIME_COMBO:
        case Schema::DAY_OF_WEEK:
        case Schema::INTERVAL:
        case Schema::DAY_TIME_COMBO:
            break;
        case Schema::TIME_OF_DAY:
            return true;
    }
    return false;
}

}
