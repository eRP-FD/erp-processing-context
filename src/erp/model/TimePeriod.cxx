/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/TimePeriod.hxx"

#include <stdexcept>
#include <chrono>
#include <ctime>
#if !defined __APPLE__ && !defined _WIN32
#include <features.h>
#endif

namespace model {

    namespace {

        /**
         * The ::localtime function uses a tm struct that is shared between threads and is reused by subsequent calls
         * to ::localtime. This allows for race conditions. Therefore use localtime_r when available and when not
         * create copy the returned tm struct to minimize the time of access of the shared tm struct to minimize the
         * probability for a race condition.
         */
        struct tm erpGmtime (const std::time_t& time_t_value)
        {
#if _POSIX_C_SOURCE >= 1 || _XOPEN_SOURCE || _BSD_SOURCE || _SVID_SOURCE || _POSIX_SOURCE
            struct tm tm_value {};
            gmtime_r(&time_t_value, &tm_value);
            return tm_value;
#else
            struct tm* tm_value = gmtime(&time_t_value);
            return *tm_value;
#endif
        }

        /**
         * The following addOne(Day|Month|Year) functions don't parameterize the number of days, months and years because
         * it is unclear how well the tm struct would handle an extreme case of adding, say, 1000 days. Therefore the
         * functions are limited on the use cases: provide the `end` value of a period that represent a single day, a
         * single month or a single year.
         *
         * The use `struct tm` is motivated by missing support for days, months and years in std::chrono in C++17 and the
         * handling of leap years (and possibly leap seconds). As we don't want to handle that ourselves we make an
         * excursion via time_t to tm, do the arithmetic and go back to time_point.
         */

        Timestamp addOneSecond (const Timestamp t)
        {
            return Timestamp(t.toChronoTimePoint() + std::chrono::seconds(1));
        }

        Timestamp addOneMinute (const Timestamp t)
        {
            return Timestamp(t.toChronoTimePoint() + std::chrono::minutes(1));
        }

        Timestamp addOneDay (const Timestamp t)
        {
            return Timestamp(t.toChronoTimePoint() + std::chrono::hours(24));
        }

        Timestamp addOneMonth (const Timestamp t)
        {
            // To tm.
            const std::time_t timeT = std::chrono::system_clock::to_time_t(t.toChronoTimePoint());
            auto tm = erpGmtime(timeT);

            // Add a single month.
            tm.tm_mon += 1;

            return Timestamp::fromTmInUtc(tm);
        }

        Timestamp addOneYear (const Timestamp t)
        {
            // To tm.
            const std::time_t timeT = std::chrono::system_clock::to_time_t(t.toChronoTimePoint());
            auto tm = erpGmtime(timeT);

            // Add a single year.
            tm.tm_year += 1;

            return Timestamp::fromTmInUtc(tm);
        }
    }

TimePeriod::TimePeriod (Timestamp start, Timestamp end)
    : mBegin(start),
      mEnd(end)
{
}


TimePeriod TimePeriod::fromFhirSearchDate (const std::string& date)
{
    const Timestamp timestamp = Timestamp::fromFhirSearchDateTime(date);
    switch (Timestamp::detectType(date))
    {
        case Timestamp::Type::DateTime:
            return TimePeriod(timestamp, addOneSecond(timestamp));

        case Timestamp::Type::Date:
            return TimePeriod(timestamp, addOneDay(timestamp));

        case Timestamp::Type::DateTimeWithoutSeconds:
            return TimePeriod(timestamp, addOneMinute(timestamp));

        case Timestamp::Type::YearMonth:
            return TimePeriod(timestamp, addOneMonth(timestamp));

        case Timestamp::Type::Year:
            return TimePeriod(timestamp, addOneYear(timestamp));

        default:
            throw std::logic_error("for search unsupported Timestamp::Type enum value");
    }
}


Timestamp TimePeriod::begin (void) const
{
    return mBegin;
}


Timestamp TimePeriod::end (void) const
{
    return mEnd;
}

}
