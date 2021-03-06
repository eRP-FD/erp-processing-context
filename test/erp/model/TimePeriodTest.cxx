/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/TimePeriod.hxx"

#include <date/date.h>
#include <gtest/gtest.h>


using namespace date;
using namespace std::literals::chrono_literals;
using namespace model;


class TimePeriodTest : public testing::Test
{
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

        EXPECT_EQ(period.begin(), Timestamp(sys_days{ January / 1 / 1970 }));
        EXPECT_EQ(period.end(), Timestamp(sys_days{ January / 2 / 1970 }));
    }

    {
        // 1 day after start of epoch.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("1970-01-02");

        EXPECT_EQ(period.begin(), Timestamp(sys_days{ January / 2 / 1970 }));
        EXPECT_EQ(period.end(), Timestamp(sys_days{ January / 3 / 1970 }));
    }

    {
        // In the middle of a year.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2022-08-21");

        EXPECT_EQ(period.begin(), Timestamp(sys_days{August/21/2022}));
        EXPECT_EQ(period.end(),   Timestamp(sys_days{August/22/2022}));
    }

    {
        // At the end of a year.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2022-12-31");

        EXPECT_EQ(period.begin(), Timestamp(sys_days{December/31/2022}));
        EXPECT_EQ(period.end(),   Timestamp(sys_days{January/01/2023}));
    }

    {
        // At the end of a month.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2022-08-31");

        EXPECT_EQ(period.begin(), Timestamp(sys_days{August/31/2022}));
        EXPECT_EQ(period.end(),   Timestamp(sys_days{September/01/2022}));
    }

    {
        // Leap year day.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2020-02-28");

        EXPECT_EQ(period.begin(), Timestamp(sys_days{February/28/2020}));
        EXPECT_EQ(period.end(),   Timestamp(sys_days{February/29/2020}));
    }

    {
        // Non-leap year leap-day.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2021-02-28");

        EXPECT_EQ(period.begin(), Timestamp(sys_days{February/28/2021}));
        EXPECT_EQ(period.end(),   Timestamp(sys_days{March/01/2021}));
    }
}


TEST_F(TimePeriodTest, fromFhirSearchDate_successForYearMonth)
{
    {
        // In the middle of a year.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2022-08");

        EXPECT_EQ(period.begin(), Timestamp(sys_days{August/01/2022}));
        EXPECT_EQ(period.end(),   Timestamp(sys_days{September/01/2022}));
    }

    {
        // At the end of a year.
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2022-12");

        EXPECT_EQ(period.begin(), Timestamp(sys_days{December/01/2022}));
        EXPECT_EQ(period.end(),   Timestamp(sys_days{January/01/2023}));
    }
}


TEST_F(TimePeriodTest, fromFhirSearchDate_successForYear)
{
    const TimePeriod period = TimePeriod::fromFhirSearchDate("2022");

    EXPECT_EQ(period.begin(), Timestamp(sys_days{January/01/2022}));
    EXPECT_EQ(period.end(),   Timestamp(sys_days{January/01/2023}));
}


TEST_F(TimePeriodTest, fromFhirSearchDate_optionalTimeZone)//NOLINT(readability-function-cognitive-complexity)
{
    {
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2022-09-07T16:44:59");

        const auto expectedDateTime = sys_days{September / 7 / 2022} + 16h + 44min + 59s;
        EXPECT_EQ(period.begin(), Timestamp(expectedDateTime));
        EXPECT_EQ(period.end(), Timestamp(expectedDateTime + 1s));

        const TimePeriod periodZ = TimePeriod::fromFhirSearchDate("2022-09-07T16:44:59Z");
        EXPECT_EQ(period.begin(), periodZ.begin());
        EXPECT_EQ(period.end(), periodZ.end());

        const TimePeriod periodTz = TimePeriod::fromFhirSearchDate("2022-09-07T16:44:59+00:00");
        EXPECT_EQ(period.begin(), periodTz.begin());
        EXPECT_EQ(period.end(), periodTz.end());
    }
    {
        // combined with missing seconds:
        const TimePeriod period = TimePeriod::fromFhirSearchDate("2022-09-07T16:44");

        const auto expectedDateTime = sys_days{September / 7 / 2022} + 16h + 44min + 0s;
        EXPECT_EQ(period.begin(), Timestamp(expectedDateTime));
        EXPECT_EQ(period.end(), Timestamp(expectedDateTime + 1min));

        const TimePeriod periodZ = TimePeriod::fromFhirSearchDate("2022-09-07T16:44Z");
        EXPECT_EQ(period.begin(), periodZ.begin());
        EXPECT_EQ(period.end(), periodZ.end());

        const TimePeriod periodTz = TimePeriod::fromFhirSearchDate("2022-09-07T18:44+02:00");
        EXPECT_EQ(period.begin(), periodTz.begin());
        EXPECT_EQ(period.end(), periodTz.end());
    }
}
