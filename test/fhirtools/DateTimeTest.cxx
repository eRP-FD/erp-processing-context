/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/model/DateTime.hxx"

#include <gtest/gtest.h>

using namespace fhirtools;


TEST(Time, PrecisionTest)
{
    Time time1("04");
    Time time2("04:00");
    Time time3("04:00:00");
    Time time4("04:00:00.000");
    EXPECT_EQ(time1.compareTo(time2), std::nullopt);
    EXPECT_EQ(time1.compareTo(time3), std::nullopt);
    EXPECT_EQ(time1.compareTo(time4), std::nullopt);
    EXPECT_EQ(time1.toString(), "04");
    EXPECT_EQ(time2.toString(), "04:00");
    EXPECT_EQ(time3.toString(), "04:00:00");
    EXPECT_EQ(time4.toString(), "04:00:00.000");

    EXPECT_EQ(Time("01:23:45.678").toString(), "01:23:45.678");
    EXPECT_EQ(Time("00:00:00.000").toString(), "00:00:00.000");
}

TEST(Time, InvalidTest)
{
    EXPECT_ANY_THROW(Time("-1"));
    EXPECT_ANY_THROW(Time("00:60:31"));
    EXPECT_ANY_THROW(Time("01:"));
    EXPECT_ANY_THROW(Time("01:01:"));
    EXPECT_ANY_THROW(Time("01:01:000."));
    EXPECT_ANY_THROW(Time("24:00:00.1"));
}

TEST(Time, Timezones)
{
    EXPECT_EQ(DateTime("2022-01-01TZ").compareTo(DateTime("2022-01-01T+00:00")), std::strong_ordering::equal);
    EXPECT_EQ(DateTime("2022-01-01T00:00:00Z").compareTo(DateTime("2022-01-01T02:00:00+02:00")),
              std::strong_ordering::equal);
    EXPECT_EQ(DateTime("2022-01-31T23:00:00+00:00").compareTo(DateTime("2022-02-01T01:00:00+02:00")),
              std::strong_ordering::equal);
    EXPECT_EQ(DateTime("2022-01-31T23:00:00+00:00").compareTo(DateTime("2022-02-01T00:00:00+02:00")),
              std::strong_ordering::greater);
    EXPECT_EQ(DateTime("2022-01-31T23:30:00+00:00").compareTo(DateTime("2022-01-31")), std::strong_ordering::greater);
}

TEST(Time, SpecificationSamples)
{
    EXPECT_NO_THROW(Time("12:00"));
    EXPECT_NO_THROW(Time("14:30:14.559"));
}

TEST(Date, PrecisionTest)
{
    Date date1("2022");
    Date date2("2022-01");
    Date date3("2022-01-01");
    EXPECT_EQ(date1.toString(), "2022");
    EXPECT_EQ(date2.toString(), "2022-01");
    EXPECT_EQ(date3.toString(), "2022-01-01");
}

TEST(Date, InvalidTest)
{
    EXPECT_ANY_THROW(Date("2022-13-01"));
    EXPECT_ANY_THROW(Date("2022-10-32"));
    EXPECT_ANY_THROW(Date("2022-11-31"));
    EXPECT_ANY_THROW(Date("-2022"));
}

TEST(Date, SpecificationSamples)
{
    EXPECT_NO_THROW(Date("2014-01-25"));
    EXPECT_NO_THROW(Date("2014-01"));
    EXPECT_NO_THROW(Date("2014"));
}

TEST(DateTime, ValidTest)
{
    EXPECT_NO_THROW(DateTime("1970"));
    EXPECT_NO_THROW(DateTime("1970-01"));
    EXPECT_NO_THROW(DateTime("1970-01-01"));
    EXPECT_NO_THROW(DateTime("1970-01-01T00"));
    EXPECT_NO_THROW(DateTime("1970-01-01T00:00"));
    EXPECT_NO_THROW(DateTime("1970-01-01T00:00:00"));
    EXPECT_NO_THROW(DateTime("1970-01-01T00:00:00Z"));
    EXPECT_NO_THROW(DateTime("1970-01-01T00:00:00+00:00"));
    EXPECT_NO_THROW(DateTime("1970-01-01T00:00:00.000"));
    EXPECT_NO_THROW(DateTime("1970-01-01T00:00:00.000Z"));
    EXPECT_NO_THROW(DateTime("1970-01-01T00:00:00.000+00:00"));
}

TEST(DateTime, ExtraCharacters)
{
    EXPECT_ANY_THROW(DateTime("1970Z"));
    EXPECT_ANY_THROW(DateTime("1970-01Z"));
    EXPECT_ANY_THROW(DateTime("1970-01-01Z"));
    EXPECT_ANY_THROW(DateTime("1970-01-01T00A"));
    EXPECT_ANY_THROW(DateTime("1970-01-01T00:00A"));
    EXPECT_ANY_THROW(DateTime("1970-01-01T00:00:00A"));
    EXPECT_ANY_THROW(DateTime("1970-01-01T00:00:00ZA"));
    EXPECT_ANY_THROW(DateTime("1970-01-01T00:00:00+00:00A"));

}

TEST(DateTime, SpecificationSamples)
{
    EXPECT_NO_THROW(DateTime("2014-01-25T14:30:14.559"));
    EXPECT_NO_THROW(DateTime("2014-01-25T14:30:14.559Z"));// A date time with UTC timezone offset
    EXPECT_NO_THROW(DateTime("2014-01-25T14:30"));        // A partial DateTime with year, month, day, hour, and minute
    EXPECT_NO_THROW(DateTime("2014-03-25T"));             // A partial DateTime with year, month, and day
    EXPECT_NO_THROW(DateTime("2014-01T"));                // A partial DateTime with year and month
    EXPECT_NO_THROW(DateTime("2014T"));                   // A partial DateTime with only the year
}

TEST(DateTime, compareTo)
{
    EXPECT_EQ(Date("2012").compareTo(Date("2012")), std::make_optional(std::strong_ordering::equal));
    EXPECT_EQ(Date("2012").compareTo(Date("2013")), std::make_optional(std::strong_ordering::less));
    EXPECT_EQ(Date("2012-01").compareTo(Date("2013")), std::make_optional(std::strong_ordering::less));
    EXPECT_EQ(Date("2013-01").compareTo(Date("2012")), std::make_optional(std::strong_ordering::greater));
    EXPECT_EQ(Date("2013-02-01").compareTo(Date("2013-01")), std::make_optional(std::strong_ordering::greater));
    EXPECT_EQ(Date("2012-01").compareTo(Date("2012")), std::nullopt);
    EXPECT_EQ(DateTime("2012").compareTo(DateTime("2012")), std::make_optional(std::strong_ordering::equal));
    EXPECT_EQ(DateTime("2014").compareTo(DateTime("2013")), std::make_optional(std::strong_ordering::greater));
    EXPECT_EQ(DateTime("2012-01").compareTo(DateTime("2012")), std::nullopt);
    EXPECT_EQ(DateTime("2014-01-25T14:30:14.559").compareTo(DateTime("2014-01-25T14:30:14.559")), std::make_optional(std::strong_ordering::equal));
}
