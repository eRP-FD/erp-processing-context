/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/TimePeriod.hxx"
#include "erp/util/Expect.hxx"

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

        fhirtools::Timestamp addOneSecond (const fhirtools::Timestamp t)
        {
            return fhirtools::Timestamp(t.toChronoTimePoint() + std::chrono::seconds(1));
        }

        fhirtools::Timestamp addOneMinute (const fhirtools::Timestamp t)
        {
            return fhirtools::Timestamp(t.toChronoTimePoint() + std::chrono::minutes(1));
        }

        fhirtools::Timestamp addOneDay (const fhirtools::Timestamp t)
        {
            return fhirtools::Timestamp(t.toChronoTimePoint() + std::chrono::hours(24));
        }

        fhirtools::Timestamp addOneMonth (const fhirtools::Timestamp t)
        {
            // To tm.
            const std::time_t timeT = std::chrono::system_clock::to_time_t(t.toChronoTimePoint());
            auto tm = erpGmtime(timeT);

            // Add a single month.
            tm.tm_mon += 1;

            return fhirtools::Timestamp::fromTmInUtc(tm);
        }

        fhirtools::Timestamp addOneYear (const fhirtools::Timestamp t)
        {
            // To tm.
            const std::time_t timeT = std::chrono::system_clock::to_time_t(t.toChronoTimePoint());
            auto tm = erpGmtime(timeT);

            // Add a single year.
            tm.tm_year += 1;

            return fhirtools::Timestamp::fromTmInUtc(tm);
        }
    }

TimePeriod::TimePeriod (fhirtools::Timestamp start, fhirtools::Timestamp end)
    : mBegin(start),
      mEnd(end)
{
}


TimePeriod TimePeriod::fromFhirSearchDate (const std::string& date)
{
    const fhirtools::Timestamp timestamp = fhirtools::Timestamp::fromFhirSearchDateTime(date);
    switch (fhirtools::Timestamp::detectType(date))
    {
        case fhirtools::Timestamp::Type::DateTime:
            return TimePeriod(timestamp, addOneSecond(timestamp));

        case fhirtools::Timestamp::Type::Date:
            return TimePeriod(timestamp, addOneDay(timestamp));

        case fhirtools::Timestamp::Type::DateTimeWithoutSeconds:
            return TimePeriod(timestamp, addOneMinute(timestamp));

        case fhirtools::Timestamp::Type::YearMonth:
            return TimePeriod(timestamp, addOneMonth(timestamp));

        case fhirtools::Timestamp::Type::Year:
            return TimePeriod(timestamp, addOneYear(timestamp));

        case fhirtools::Timestamp::Type::DateTimeWithFractionalSeconds:
            break;
    }
    Fail2("for search unsupported fhirtools::Timestamp::Type enum value", std::logic_error);
}


fhirtools::Timestamp TimePeriod::begin (void) const
{
    return mBegin;
}


fhirtools::Timestamp TimePeriod::end (void) const
{
    return mEnd;
}

}
