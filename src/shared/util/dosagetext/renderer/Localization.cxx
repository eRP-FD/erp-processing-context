/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/dosagetext/renderer/Localization.hxx"
#include "shared/util/String.hxx"

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim_all.hpp>
#include <iomanip>
#include <sstream>

namespace dosagetext
{

bool isSingular(GrammaticalNumber form)
{
    switch (form)
    {
        case GrammaticalNumber::SINGULAR:
            return true;
        case GrammaticalNumber::PLURAL:
            break;
    }
    return false;
}

std::string_view germanTimeUnit(model::TimingDpMpTimeUnit timeUnit, GrammaticalNumber form)
{
    switch (timeUnit)
    {
        case model::TimingDpMpTimeUnit::s:
            return isSingular(form) ? "Sekunde" : "Sekunden";
        case model::TimingDpMpTimeUnit::min:
            return isSingular(form) ? "Minute" : "Minuten";
        case model::TimingDpMpTimeUnit::h:
            return isSingular(form) ? "Stunde" : "Stunden";
        case model::TimingDpMpTimeUnit::d:
            return isSingular(form) ? "Tag" : "Tage";
        case model::TimingDpMpTimeUnit::wk:
            return isSingular(form) ? "Woche" : "Wochen";
        case model::TimingDpMpTimeUnit::mo:
            return isSingular(form) ? "Monat" : "Monate";
        case model::TimingDpMpTimeUnit::a:
            return isSingular(form) ? "Jahr" : "Jahre";
    }
    ModelFail("invalid timeUnit: " + std::to_string(static_cast<uintmax_t>(timeUnit)));
}

std::string_view germanFrequency(model::TimingDpMpTimeUnit timeUnit)
{
    switch (timeUnit)
    {
        case model::TimingDpMpTimeUnit::s:
            return "sekündlich";
        case model::TimingDpMpTimeUnit::min:
            return "minütlich";
        case model::TimingDpMpTimeUnit::h:
            return "stündlich";
        case model::TimingDpMpTimeUnit::d:
            return "täglich";
        case model::TimingDpMpTimeUnit::wk:
            return "wöchentlich";
        case model::TimingDpMpTimeUnit::mo:
            return "monatlich";
        case model::TimingDpMpTimeUnit::a:
            return "jährlich";
    }
    ModelFail("invalid timeUnit: " + std::to_string(static_cast<uintmax_t>(timeUnit)));
}

std::string_view germanWeekdayAdverb(model::DaysOfWeek day)
{
    switch (day)
    {
        case model::DaysOfWeek::mon:
            return "montags";
        case model::DaysOfWeek::tue:
            return "dienstags";
        case model::DaysOfWeek::wed:
            return "mittwochs";
        case model::DaysOfWeek::thu:
            return "donnerstags";
        case model::DaysOfWeek::fri:
            return "freitags";
        case model::DaysOfWeek::sat:
            return "samstags";
        case model::DaysOfWeek::sun:
            return "sonntags";
    }
    ModelFail("invalid day: " + std::to_string(static_cast<uintmax_t>(day)));
}

std::string_view germanEventTiming(model::EventTiming eventTiming)
{
    switch (eventTiming)
    {
        case model::EventTiming::MORN:
            return "morgens";
        case model::EventTiming::NOON:
            return "mittags";
        case model::EventTiming::EVE:
            return "abends";
        case model::EventTiming::NIGHT:
            return "nachts";
    }
    ModelFail("invalid eventTiming: " + std::to_string(static_cast<uintmax_t>(eventTiming)));
}

std::string germanDecimal(std::string_view number)
{
    ModelExpect(! number.empty(), "number is empty");

    bool isNegative = number.starts_with('-');
    if (isNegative || number.starts_with('+'))
    {
        number = number.substr(1);
    }
    std::vector<std::string_view> parts;
    boost::split(parts, number, [](const char c) {
        return c == '.';
    });
    ModelExpect(parts.size() <= 2, "number contains more than one decimal point: " + std::string{number});
    const auto isZero = [](const char c) {
        return c == '0';
    };
    auto integer = parts[0];
    integer = boost::trim_left_copy_if(integer, isZero);
    if (integer.empty())
    {
        integer = "0";
    }
    using namespace std::string_literals;
    if (parts.size() == 2)
    {
        auto decimals = parts[1];
        decimals = boost::trim_right_copy_if(decimals, isZero);
        if (! decimals.empty())
        {
            return (isNegative ? "-"s : ""s).append(integer).append(",").append(decimals);
        }
    }
    return (integer != "0" && isNegative ? "-"s : ""s).append(integer);
}

std::string germanTimeOfDay(std::string_view timeOfDay)
{
    try
    {
        auto timeParts = String::split(timeOfDay, ':');

        const auto& hourStr = timeParts.at(0);
        auto minute = timeParts.size() > 1 ? timeParts.at(1) : "00";

        int const hour = std::stoi(hourStr);

        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << hour << ":" << minute << " Uhr";
        return oss.str();
    }
    catch (const std::exception&)
    {
        // Fallback: return original string if parsing fails
        return std::string{timeOfDay};
    }
}

std::string_view englishInvalidOrUnsupported()
{
    return "This is an invalid or unsupported file, no text was generated by the DosageGeneration Script";
}

}
