/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "Holidays.hxx"

#include <sstream>

#include "erp/util/Expect.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/String.hxx"

#include "erp/util/TLog.hxx"

namespace
{
int offsetFromEaster(NamedHolidays holiday)
{
    switch (holiday)
    {

        case NamedHolidays::Karfreitag:
            return -2;
        case NamedHolidays::Ostermontag:
            return 1;
        case NamedHolidays::Himmelfahrt:
            return 39;
        case NamedHolidays::Pfingstmontag:
            return 50;
    }
    Fail("unitialized enum value");
}

}

Holidays::Holidays()
{
    const auto& configuration = Configuration::instance();
    loadEasterDates(configuration);
    loadConfiguredHolidays(configuration);
}

void Holidays::loadEasterDates(const Configuration& configuration)
{
    const auto csvPath = configuration.getPathValue(ConfigurationKey::SERVICE_TASK_ACTIVATE_EASTER_CSV);
    Expect(std::filesystem::is_regular_file(csvPath), "easter csv not found: " + csvPath.string());
    TVLOG(1) << "loading easter dates: " << csvPath.string();
    const auto csvFile = FileHelper::readFileAsString(csvPath);
    const auto parts = String::split(csvFile, ',');
    for (const auto& part : parts)
    {
        const auto trimmed = String::trim(part);
        std::istringstream stream(trimmed);
        date::year_month_day yearMonthDay{};
        date::from_stream(stream, "%F", yearMonthDay);
        Expect(yearMonthDay.ok() && ! stream.fail() &&
                   (stream.eof() || gsl::narrow<size_t>(stream.tellg()) == trimmed.size()),
               "Error parsing cvs entry: " + part);
        const auto [it, inserted] = mEasterDates.try_emplace(yearMonthDay.year(), yearMonthDay);
        Expect(inserted, "duplicate entry in csv: " + part);
        (void) it;
    }
}


void Holidays::loadConfiguredHolidays(const Configuration& configuration)
{
    const auto configuredHolidays = configuration.getArray(ConfigurationKey::SERVICE_TASK_ACTIVATE_HOLIDAYS);
    for (const auto& configuredHoliday : configuredHolidays)
    {
        const auto trimmed = String::trim(configuredHoliday);
        if (trimmed.empty())
        {
            // no expect to still support completely empty config, i.e. no holidays
            TLOG(WARNING) << "empty holiday configuration";
            continue;
        }
        const auto enumValue = magic_enum::enum_cast<NamedHolidays>(trimmed);
        if (enumValue)
        {
            mRelativeToEasterDates.emplace(offsetFromEaster(*enumValue));
        }
        else
        {
            // offset configured as number in config:
            try
            {
                size_t pos = 0;
                const int offsetToEaster = std::stoi(trimmed, &pos, 10);
                if (pos == trimmed.size())
                {
                    mRelativeToEasterDates.emplace(offsetToEaster);
                    continue;
                }
            }
            catch (const std::invalid_argument&)
            {
                // on std::stoi errors, continue trying to parse the entry
            }

            std::istringstream stream(trimmed);
            date::month_day fixedDate{};
            date::from_stream(stream, "%m-%d", fixedDate);
            Expect(fixedDate.ok() && ! stream.fail() &&
                       (stream.eof() || gsl::narrow<size_t>(stream.tellg()) == trimmed.size()),
                   "Failed to parse holiday date " + configuredHoliday);
            mFixedDates.emplace(fixedDate);
        }
    }
}


const Holidays& Holidays::instance()
{
    static Holidays instance;
    return instance;
}


bool Holidays::isHoliday(date::year_month_day yearMonthDay) const
{
    const date::month_day monthDay(yearMonthDay.month(), yearMonthDay.day());
    if (mFixedDates.count(monthDay) > 0)
        return true;

    const auto year = yearMonthDay.year();
    const auto easterDate = mEasterDates.find(year);
    Expect(easterDate != mEasterDates.end(),
           "No easter date configured for year " + std::to_string(static_cast<int>(year)));

    if (easterDate->second == yearMonthDay)
        return true;

    const auto offset = date::sys_days{yearMonthDay} - date::sys_days{easterDate->second};
    return mRelativeToEasterDates.count(offset) > 0;
}
