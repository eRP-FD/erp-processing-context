/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/TimePeriod.hxx"

#include <date/date.h>
#include <date/tz.h>
#include <gtest/gtest.h>


using namespace date;
using namespace std::literals::chrono_literals;
using namespace model;

using model::Timestamp;

class TimePeriodTest : public testing::Test
{
protected:
    template <class Duration>
    Timestamp fromGermanTime(local_time<Duration> duration)
    {
        return Timestamp(make_zoned(Timestamp::GermanTimezone, duration).get_sys_time());
    }
};

TEST_F(TimePeriodTest, fromFhirSearchDate_successForDateTime)//NOLINT(readability-function-cognitive-complexity)
{
    {
        // Start of epoch.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("1970-01-01T00:00:00Z");

        const auto expectedDateTime = sys_days{ January / 1 / 1970 } + 0h + 0min + 0s;
        EXPECT_EQ(period.begin(), Timestamp(expectedDateTime));
        EXPECT_EQ(period.end(), Timestamp(expectedDateTime + 1s));
    }

    {
        // 1 hour after start of epoch.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("1970-01-01T01:00:00Z");

        const auto expectedDateTime = sys_days{ January / 1 / 1970 } + 1h + 0min + 0s;
        EXPECT_EQ(period.begin(), Timestamp(expectedDateTime));
        EXPECT_EQ(period.end(), Timestamp(expectedDateTime + 1s));
    }

    {
        // 1 day after start of epoch.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("1970-01-02T00:00:00Z");

        const auto expectedDateTime = sys_days{ January / 2 / 1970 } + 0h + 0min + 0s;
        EXPECT_EQ(period.begin(), Timestamp(expectedDateTime));
        EXPECT_EQ(period.end(), Timestamp(expectedDateTime + 1s));
    }

    {
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2022-08-21T23:01:23Z");

        const auto expectedDateTime = sys_days{August/21/2022} + 23h + 1min + 23s;
        EXPECT_EQ(period.begin(), Timestamp(expectedDateTime));
        EXPECT_EQ(period.end(), Timestamp(expectedDateTime + 1s));
    }
}


TEST_F(TimePeriodTest, fromFhirSearchDate_successForDate)//NOLINT(readability-function-cognitive-complexity)
{
    {
        // Start of epoch.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("1970-01-01");

        EXPECT_EQ(period.begin(), fromGermanTime(local_days{January / 1 / 1970}));
        EXPECT_EQ(period.end(), fromGermanTime(local_days{January / 2 / 1970}));
    }

    {
        // 1 day after start of epoch.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("1970-01-02");

        EXPECT_EQ(period.begin(), fromGermanTime(local_days{ January / 2 / 1970 }));
        EXPECT_EQ(period.end(), fromGermanTime(local_days{ January / 3 / 1970 }));
    }

    {
        // In the middle of a year.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2022-08-21");

        EXPECT_EQ(period.begin(), fromGermanTime(local_days{August/21/2022}));
        EXPECT_EQ(period.end(),   fromGermanTime(local_days{August/22/2022}));
    }

    {
        // At the end of a year.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2022-12-31");

        EXPECT_EQ(period.begin(), fromGermanTime(local_days{December/31/2022}));
        EXPECT_EQ(period.end(),   fromGermanTime(local_days{January/01/2023}));
    }

    {
        // At the end of a month.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2022-08-31");

        EXPECT_EQ(period.begin(), fromGermanTime(local_days{August/31/2022}));
        EXPECT_EQ(period.end(),   fromGermanTime(local_days{September/01/2022}));
    }

    {
        // Leap year day.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2020-02-28");

        EXPECT_EQ(period.begin(), fromGermanTime(local_days{February/28/2020}));
        EXPECT_EQ(period.end(),   fromGermanTime(local_days{February/29/2020}));
    }

    {
        // Non-leap year leap-day.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2021-02-28");

        EXPECT_EQ(period.begin(), fromGermanTime(local_days{February/28/2021}));
        EXPECT_EQ(period.end(),   fromGermanTime(local_days{March/01/2021}));
    }
}


TEST_F(TimePeriodTest, fromFhirSearchDate_successForYearMonth)
{
    {
        // In the middle of a year.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2022-08");

        EXPECT_EQ(period.begin(), fromGermanTime(local_days{August/01/2022}));
        EXPECT_EQ(period.end(),   fromGermanTime(local_days{September/01/2022}));
    }

    {
        // At the end of a year.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2022-12");

        EXPECT_EQ(period.begin(), fromGermanTime(local_days{December/01/2022}));
        EXPECT_EQ(period.end(),   fromGermanTime(local_days{January/01/2023}));
    }
}


TEST_F(TimePeriodTest, fromFhirSearchDate_successForYear)
{
    const TimePeriod period = TimePeriod::fromFhirSearchDate("2022");

    EXPECT_EQ(period.begin(), fromGermanTime(local_days{January/01/2022}));
    EXPECT_EQ(period.end(),   fromGermanTime(local_days{January/01/2023}));
}


TEST_F(TimePeriodTest, fromFhirSearchDate_optionalTimeZone)//NOLINT(readability-function-cognitive-complexity)
{
    {
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2022-09-07T16:44:59");

        const auto expectedDateTime = local_days{September / 7 / 2022} + 16h + 44min + 59s;
        EXPECT_EQ(period.begin(), fromGermanTime(expectedDateTime));
        EXPECT_EQ(period.end(), fromGermanTime(expectedDateTime + 1s));

        const TimePeriod periodZ = TimePeriod::fromFhirSearchDate("2022-09-07T14:44:59Z");
        EXPECT_EQ(period.begin(), periodZ.begin());
        EXPECT_EQ(period.end(), periodZ.end());

        const TimePeriod periodTz = TimePeriod::fromFhirSearchDate("2022-09-07T14:44:59+00:00");
        EXPECT_EQ(period.begin(), periodTz.begin());
        EXPECT_EQ(period.end(), periodTz.end());
    }
    {
        // combined with missing seconds:
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2022-09-07T16:44");

        const auto expectedDateTime = local_days{September / 7 / 2022} + 16h + 44min + 0s;
        EXPECT_EQ(period.begin(), fromGermanTime(expectedDateTime));
        EXPECT_EQ(period.end(), fromGermanTime(expectedDateTime + 1min));

        const TimePeriod periodZ = TimePeriod::fromFhirSearchDate("2022-09-07T14:44Z");
        EXPECT_EQ(period.begin(), periodZ.begin());
        EXPECT_EQ(period.end(), periodZ.end());

        const TimePeriod periodTz = TimePeriod::fromFhirSearchDate("2022-09-07T16:44+02:00");
        EXPECT_EQ(period.begin(), periodTz.begin());
        EXPECT_EQ(period.end(), periodTz.end());
    }
}

TEST_F(TimePeriodTest, fromDatabaseUuid_success)
{
    {
        //const TimePeriod period = TimePeriod::fromDatabaseUuid("01ebc91d-3b36-a2b8-590e-7045e1630593");
        const auto *const dbUuid = "01ebc91d-3b2d-d0e8-0000-000000000000";
        const TimePeriod period = TimePeriod::fromDatabaseUuid(dbUuid);
        const auto converted = period.begin().toDatabaseSUuid();
        EXPECT_EQ(dbUuid, converted);
    }
}
