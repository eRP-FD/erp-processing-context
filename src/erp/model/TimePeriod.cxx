/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/TimePeriod.hxx"
#include "erp/util/Expect.hxx"

#include <stdexcept>
#include <chrono>
#include <ctime>
#include <date/tz.h>
#if !defined __APPLE__ && !defined _WIN32
#include <features.h>
#endif

namespace model {

    namespace {

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

        model::Timestamp addOneSecond (const model::Timestamp t)
        {
            return model::Timestamp(t.toChronoTimePoint() + std::chrono::seconds(1));
        }

        model::Timestamp addOneMinute (const model::Timestamp t)
        {
            return model::Timestamp(t.toChronoTimePoint() + std::chrono::minutes(1));
        }

        model::Timestamp addOneDay (const model::Timestamp t)
        {
            return model::Timestamp(t.toChronoTimePoint() + std::chrono::hours(24));
        }

        model::Timestamp addOneMonth (const model::Timestamp t)
        {
            using zoned_ms = date::zoned_time<std::chrono::milliseconds>;
            auto zt = zoned_ms{Timestamp::GermanTimezone, t.toChronoTimePoint()};
            auto localDay = date::floor<date::days>(zt.get_local_time());
            auto timeOfDay = zt.get_local_time() - localDay;
            auto ymd = date::year_month_day{localDay} + date::months{1};
            if (! ymd.ok())
                ymd = ymd.year() / ymd.month() / date::last;
            zt = date::local_days{ymd} + timeOfDay;
            return Timestamp{zt.get_sys_time()};
        }

        model::Timestamp addOneYear (const model::Timestamp t)
        {

            using zoned_ms = date::zoned_time<std::chrono::milliseconds>;
            auto zt = zoned_ms{Timestamp::GermanTimezone, t.toChronoTimePoint()};
            auto localDay = date::floor<date::days>(zt.get_local_time());
            auto timeOfDay = zt.get_local_time() - localDay;
            auto ymd = date::year_month_day{localDay} + date::years{1};
            if (! ymd.ok())
                ymd = ymd.year() / ymd.month() / date::last;
            zt = date::local_days{ymd} + timeOfDay;
            return Timestamp{zt.get_sys_time()};
        }
    }

TimePeriod::TimePeriod (model::Timestamp start, model::Timestamp end)
    : mBegin(start),
      mEnd(end)
{
}


TimePeriod TimePeriod::fromFhirSearchDate (const std::string& date)
{
    const model::Timestamp timestamp = model::Timestamp::fromFhirSearchDateTime(date);
    switch (model::Timestamp::detectType(date))
    {
        case model::Timestamp::Type::DateTime:
            return TimePeriod(timestamp, addOneSecond(timestamp));

        case model::Timestamp::Type::Date:
            return TimePeriod(timestamp, addOneDay(timestamp));

        case model::Timestamp::Type::DateTimeWithoutSeconds:
            return TimePeriod(timestamp, addOneMinute(timestamp));

        case model::Timestamp::Type::YearMonth:
            return TimePeriod(timestamp, addOneMonth(timestamp));

        case model::Timestamp::Type::Year:
            return TimePeriod(timestamp, addOneYear(timestamp));

        case model::Timestamp::Type::DateTimeWithFractionalSeconds:
            break;
    }
    Fail2("for search unsupported model::Timestamp::Type enum value", std::logic_error);
}


TimePeriod TimePeriod::fromDatabaseUuid(const std::string& uuid)
{
    const auto timestamp = model::Timestamp::fromDatabaseSUuid(uuid);
    // the database suuid is always a dateTime timstamp
    return TimePeriod(timestamp, model::Timestamp(timestamp.toChronoTimePoint() + std::chrono::microseconds(1)));
}


model::Timestamp TimePeriod::begin (void) const
{
    return mBegin;
}


model::Timestamp TimePeriod::end (void) const
{
    return mEnd;
}

}
