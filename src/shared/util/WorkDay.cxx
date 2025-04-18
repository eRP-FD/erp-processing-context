/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/WorkDay.hxx"

#include "shared/util/Expect.hxx"
#include "shared/util/Holidays.hxx"

#include <date/tz.h>

WorkDay::WorkDay(model::Timestamp baseTime)
    : WorkDay(date::year_month_day{std::chrono::time_point_cast<date::sys_days::duration>(
          date::make_zoned(model::Timestamp::GermanTimezone, baseTime.toChronoTimePoint()).get_local_time())})
{
}

WorkDay::WorkDay(const date::year_month_day& day)
    : mDay(day)
{
    while (! isWorkDay(mDay))
    {
        mDay = date::sys_days{mDay} + date::days{1};
    }
}


WorkDay WorkDay::operator+(unsigned workingDaysToAdd)
{
    Expect3(isWorkDay(mDay), "mDay is not a work day", std::logic_error);
    date::year_month_day day(mDay);
    while (workingDaysToAdd > 0)
    {
        day = date::sys_days{day} + date::days{1};

        // For each non-work day the days to be added increases by one.
        if (isWorkDay(day))
            --workingDaysToAdd;
    }
    return WorkDay(day);
}


model::Timestamp WorkDay::toTimestamp() const
{
    return model::Timestamp(date::sys_days(mDay));
}


date::year_month_day WorkDay::getYearMonthDay() const
{
    return mDay;
}


bool WorkDay::isWorkDay(const date::year_month_day& day)
{
    date::weekday weekday(day);
    if (weekday == date::Sunday)
        return false;
    return ! Holidays::instance().isHoliday(day);
}
