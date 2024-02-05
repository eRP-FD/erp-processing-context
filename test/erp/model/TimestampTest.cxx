/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Timestamp.hxx"
#include "erp/model/ModelException.hxx"
#include "erp/util/TLog.hxx"

#include <date/date.h>
#include <gtest/gtest.h>


using namespace date;
using namespace std::literals::chrono_literals;
using namespace model;
using model::Timestamp;

class TimestampTest : public testing::Test
{
public:
    //NOLINTNEXTLINE(readability-function-cognitive-complexity)
    static void expectErrorForValidTimezones (
        const std::string& validDate,
        const std::function<void(const std::string& s)>& consumer)
    {
        // With UTC timezones
        EXPECT_THROW(consumer(validDate + "Z"), ModelException);
        EXPECT_THROW(consumer(validDate + "+00:00"), ModelException);
        EXPECT_THROW(consumer(validDate + "-00:00"), ModelException);

        // With other timezones, positive.
        EXPECT_THROW(consumer(validDate + "+01:00"), ModelException);
        EXPECT_THROW(consumer(validDate + "+01:23"), ModelException);
        EXPECT_THROW(consumer(validDate + "+13:59"), ModelException);

        // With other timezones, negative.
        EXPECT_THROW(consumer(validDate + "-01:00"), ModelException);
        EXPECT_THROW(consumer(validDate + "-01:23"), ModelException);
        EXPECT_THROW(consumer(validDate + "-13:59"), ModelException);
    }

    //NOLINTNEXTLINE(readability-function-cognitive-complexity)
    static void expectErrorForInvalidTimezones (
        const std::string& validDate,
        const std::function<void(const std::string& s)>& consumer)
    {
        // Lowercase z is not supported. Nor are whitespaces before Z.
        EXPECT_THROW(consumer(validDate + "z"), ModelException);
        EXPECT_THROW(consumer(validDate + " Z"), ModelException);

        // Additional characters are not supported.
        EXPECT_THROW(consumer(validDate + " "), ModelException);
        EXPECT_THROW(consumer(validDate + " ?"), ModelException);
        EXPECT_THROW(consumer(validDate + " this is a date"), ModelException);

        // Partial timezone offsets are not supported.
        EXPECT_THROW(consumer(validDate + "+01"), ModelException);
        EXPECT_THROW(consumer(validDate + "-0:00"), ModelException);
        EXPECT_THROW(consumer(validDate + "+01:0"), ModelException);
        EXPECT_THROW(consumer(validDate + " 00:00"), ModelException);
        EXPECT_THROW(consumer(validDate + "00:00"), ModelException);

        // Timezone offsets outside +/- 14:00 are invalid.
        EXPECT_THROW(consumer(validDate + "+14:01"), ModelException);
        EXPECT_THROW(consumer(validDate + "-14:01"), ModelException);
        EXPECT_THROW(consumer(validDate + "+23:00"), ModelException);

        // Minute values above 59 are invalid.
        EXPECT_THROW(consumer(validDate + "+00:60"), ModelException);
        EXPECT_THROW(consumer(validDate + "+00:99"), ModelException);
    }
};


TEST_F(TimestampTest, fromXsDateTime_success)
{
    const auto expectedDateTime = Timestamp(sys_days{January/29/2022} + 12h + 34min + 56s);

    EXPECT_EQ(Timestamp::fromXsDateTime("2022-01-29T12:34:56Z"), expectedDateTime);
    EXPECT_EQ(Timestamp::fromXsDateTime("2022-01-29T12:34:56+00:00"), expectedDateTime);
    EXPECT_EQ(Timestamp::fromXsDateTime("2022-01-29T12:34:56-00:00"), expectedDateTime);

    // Time zone plus one, but hour minus one, cancel each other out.
    EXPECT_EQ(Timestamp::fromXsDateTime("2022-01-29T11:34:56-01:00"), expectedDateTime);
    EXPECT_NO_THROW(Timestamp::fromXsDateTime("0001-01-01T00:00:00Z"));
    EXPECT_EQ(Timestamp::fromXsDateTime("0001-01-01T00:00:00Z").toXsDateTime(), "0001-01-01T00:00:00.000+00:00");
    EXPECT_EQ(Timestamp::fromXsDateTime("0001-01-01T00:00:00Z").toXsDateTimeWithoutFractionalSeconds(),
              "0001-01-01T00:00:00+00:00");
}


TEST_F(TimestampTest, fromXsDateTime_successWithMilliseconds)
{
    const auto expectedDateTime = Timestamp(sys_days{January/29/2022} + 12h + 34min + 56s + 123ms);

    EXPECT_EQ(Timestamp::fromXsDateTime("2022-01-29T12:34:56.123Z"), expectedDateTime);
}

TEST_F(TimestampTest, fromXsDateTime_successWithNanoseconds)
{
    EXPECT_NO_THROW(Timestamp::fromXsDateTime("2022-01-29T12:34:56.123456Z"));
    EXPECT_NO_THROW(Timestamp::fromXsDateTime("2022-01-29T12:34:56.123456+01:00"));
    EXPECT_NO_THROW(Timestamp::fromXsDateTime("2022-01-29T12:34:56.123456-01:00"));
}


TEST_F(TimestampTest, fromXsDateTime_failForInvalidFormats)//NOLINT(readability-function-cognitive-complexity)
{
    // In some ISO8601 profiles a divider other than T is allowed. Not in ours.
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29 12:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29t12:34:56Z"), ModelException);

    // The std::chrono parser allows fields to omit leading zeros. That is not permitted by FHIR.
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-1-29T12:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("22-01-29T12:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-2T12:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T2:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T12:4:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T12:34:6Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T12:34:56+1:00"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T12:34:56+01:0"), ModelException);

    // Negative parts are not permitted.
    EXPECT_THROW(Timestamp::fromXsDateTime("-2022-01-29T12:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022--01-29T12:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01--29T12:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T-12:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T12:-34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T12:34:-56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T12:34:56.-0100Z"), ModelException);

    // Empty sub second value is not permitted.
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T12:34:56.Z"), ModelException);

    // Missing seconds are not permitted.
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T12:34Z"), ModelException);

    // Missing time zone is not permitted.
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T12:34:56"), ModelException);

    // invalid dates and times are not permitted
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-13-29T12:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-32T12:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T24:00:00Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T25:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T25:60:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T25:61:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T25:34:60Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T25:34:61Z"), ModelException);
    EXPECT_THROW(Timestamp::fromXsDateTime("1754-08-30T22:43:42.-871+00:00"), ModelException);
}


TEST_F(TimestampTest, fromXsDateTime_failWithoutTimeZone)
{
    EXPECT_THROW(Timestamp::fromXsDateTime("2022-01-29T12:34:56"), ModelException);
}


TEST_F(TimestampTest, fromXsDateTime_withTimeZone)
{
    // Time zones are normalized so that internally UTC is used.

    // Compare two different time zones with compensation in the hours.
    EXPECT_EQ(
        Timestamp::fromXsDateTime("2022-01-29T12:34:56+00:00"),
        Timestamp::fromXsDateTime("2022-01-29T13:34:56+01:00"));

    // Compare two different time zones without compensation in the hours.
    EXPECT_GT(
        Timestamp::fromXsDateTime("2022-01-29T12:34:56+00:00"),
        Timestamp::fromXsDateTime("2022-01-29T12:34:56+01:00"));
}


TEST_F(TimestampTest, toXsDateTime)
{
    EXPECT_EQ(Timestamp(sys_days{January/29/2022} + 12h + 34min + 56s + 123ms).toXsDateTime(), "2022-01-29T12:34:56.123+00:00");
    EXPECT_EQ(Timestamp(sys_days{August / 30 / 1754} + 22h + 43min + 42s + 123ms).toXsDateTime(),
              "1754-08-30T22:43:42.123+00:00");
}

TEST_F(TimestampTest, toXsDate)
{
    EXPECT_EQ(Timestamp(sys_days{January / 29 / 2022} + 12h + 34min + 56s + 123ms).toXsDate(Timestamp::UTCTimezone),
              "2022-01-29");
}


TEST_F(TimestampTest, toXsGYearMonth)
{
    EXPECT_EQ(
        Timestamp(sys_days{January / 29 / 2022} + 12h + 34min + 56s + 123ms).toXsGYearMonth(Timestamp::UTCTimezone),
        "2022-01");
}


TEST_F(TimestampTest, toXsGYear)
{
    EXPECT_EQ(Timestamp(sys_days{January / 29 / 2022} + 12h + 34min + 56s + 123ms).toXsGYear(Timestamp::UTCTimezone),
              "2022");
}


TEST_F(TimestampTest, now)
{
    EXPECT_LT(abs(std::chrono::duration_cast<std::chrono::seconds>(Timestamp::now().toChronoTimePoint() -
                                                                   std::chrono::system_clock::now()))
                  .count(),
              std::chrono::seconds(1).count());
}


TEST_F(TimestampTest, fromXsDate_success)
{
    const auto expectedDateTime = Timestamp(sys_days{January/29/2022});

    // Without timezone.
    EXPECT_EQ(Timestamp::fromXsDate("2022-01-29", Timestamp::UTCTimezone), expectedDateTime);
}

TEST_F(TimestampTest, fromDtmDateTime_success)
{
    const auto expectedDateTime = Timestamp(sys_days{August / 6 / 2019} + 5h + 4min + 3s);
    EXPECT_EQ(Timestamp::fromDtmDateTime("20190806050403"), expectedDateTime);
}


TEST_F(TimestampTest, fromXsDate_failForInvalidDates)//NOLINT(readability-function-cognitive-complexity)
{
    // Year must not be negative. While this is allowed by strict xs:date, FHRE does not support it.
    EXPECT_THROW(Timestamp::fromXsDate("-2022-01-29", Timestamp::UTCTimezone), ModelException);

    // Year must consist of four digits.
    EXPECT_THROW(Timestamp::fromXsDate("22-01-29", Timestamp::UTCTimezone), ModelException);
    EXPECT_THROW(Timestamp::fromXsDate("022-01-29", Timestamp::UTCTimezone), ModelException);
    EXPECT_THROW(Timestamp::fromXsDate("20022-01-29", Timestamp::UTCTimezone), ModelException);

    // Month must consist of two digits in the range of 01 to 12.
    EXPECT_THROW(Timestamp::fromXsDate("2022-1-29", Timestamp::UTCTimezone), ModelException);
    EXPECT_THROW(Timestamp::fromXsDate("2022-00-29", Timestamp::UTCTimezone), ModelException);
    EXPECT_THROW(Timestamp::fromXsDate("2022-13-29", Timestamp::UTCTimezone), ModelException);

    // Day must consist of two digits in the range of 00 to 31.
    EXPECT_THROW(Timestamp::fromXsDate("2022-01-1", Timestamp::UTCTimezone), ModelException);
    EXPECT_THROW(Timestamp::fromXsDate("2022-01-00", Timestamp::UTCTimezone), ModelException);
    EXPECT_THROW(Timestamp::fromXsDate("2022-01-32", Timestamp::UTCTimezone), ModelException);
}


TEST_F(TimestampTest, fromXsDate_failForTimezones)
{
    expectErrorForValidTimezones("2022-01-29", [](auto s) {
        Timestamp::fromXsDate(s, Timestamp::UTCTimezone);
    });
    expectErrorForInvalidTimezones("2022-01-29", [](auto s) {
        Timestamp::fromXsDate(s, Timestamp::UTCTimezone);
    });
}


TEST_F(TimestampTest, fromXsGYearMonth_success)
{
    auto expectedDateTime = Timestamp(sys_days{January/01/2022});

    // Without timezone.
    EXPECT_EQ(Timestamp::fromXsGYearMonth("2022-01", Timestamp::UTCTimezone), expectedDateTime);
}


TEST_F(TimestampTest, fromXsGYearMonth_failForInvalidDates)//NOLINT(readability-function-cognitive-complexity)
{
    // Year must not be negative. While this is allowed by strict xs:date, FHRE does not support it.
    EXPECT_THROW(Timestamp::fromXsGYearMonth("-2022-01", Timestamp::UTCTimezone), ModelException);

    // Year must consist of four digits.
    EXPECT_THROW(Timestamp::fromXsGYearMonth("22-01", Timestamp::UTCTimezone), ModelException);
    EXPECT_THROW(Timestamp::fromXsGYearMonth("022-01", Timestamp::UTCTimezone), ModelException);
    EXPECT_THROW(Timestamp::fromXsGYearMonth("20022-01", Timestamp::UTCTimezone), ModelException);

    // Month must consist of two digits in the range of 01 to 12.
    EXPECT_THROW(Timestamp::fromXsGYearMonth("2022-1", Timestamp::UTCTimezone), ModelException);
    EXPECT_THROW(Timestamp::fromXsGYearMonth("2022-00", Timestamp::UTCTimezone), ModelException);
    EXPECT_THROW(Timestamp::fromXsGYearMonth("2022-13", Timestamp::UTCTimezone), ModelException);
}


TEST_F(TimestampTest, fromGYearMonth_failForTimezones)
{
    expectErrorForValidTimezones("2022-01", [](auto s){Timestamp::fromXsGYearMonth(s, Timestamp::UTCTimezone);});
    expectErrorForInvalidTimezones("2022-01", [](auto s){Timestamp::fromXsGYearMonth(s, Timestamp::UTCTimezone);});
}


TEST_F(TimestampTest, fromXsGYear_success)
{
    auto expectedDateTime = sys_days{January / 01 / 2022};

    // Without timezone.
    EXPECT_EQ(Timestamp::fromXsGYear("2022", Timestamp::UTCTimezone), Timestamp(expectedDateTime));

    // Note that the range of the dates, covered by time_point is not specified.
    // These value have been determined experimentally.
    // Adjusting these values (that make the intervall smaller) is OK as long as a sensible range is maintained.
    EXPECT_EQ(Timestamp::fromXsGYear("1678", Timestamp::UTCTimezone), Timestamp(sys_days{January / 01 / 1678}));
    EXPECT_EQ(Timestamp::fromXsGYear("2262", Timestamp::UTCTimezone), Timestamp(sys_days{January / 01 / 2262}));
}


TEST_F(TimestampTest, fromXsGYear_failForInvalidDates)//NOLINT(readability-function-cognitive-complexity)
{
    // Year must not be negative. While this is allowed by strict xs:date, FHIR does not support it.
    EXPECT_THROW(Timestamp::fromXsGYear("-2022", Timestamp::UTCTimezone), ModelException);

    // Year must consist of four digits.
    EXPECT_THROW(Timestamp::fromXsGYear("22", Timestamp::UTCTimezone), ModelException);
    EXPECT_THROW(Timestamp::fromXsGYear("022", Timestamp::UTCTimezone), ModelException);
    EXPECT_THROW(Timestamp::fromXsGYear("20022", Timestamp::UTCTimezone), ModelException);
}


TEST_F(TimestampTest, fromGYear_failForTimezones)
{
    expectErrorForValidTimezones("2022", [](auto s){Timestamp::fromXsGYear(s, Timestamp::UTCTimezone);});
    expectErrorForInvalidTimezones("2022", [](auto s){Timestamp::fromXsGYear(s, Timestamp::UTCTimezone);});
}


TEST_F(TimestampTest, fromFhir_success)//NOLINT(readability-function-cognitive-complexity)
{
    {
        // xs:dateTime without sub-second precision.
        const auto expectedDateTime = Timestamp(sys_days{January/29/2022} + 12h + 34min + 56s);
        EXPECT_EQ(Timestamp::fromFhirDateTime("2022-01-29T12:34:56Z"), expectedDateTime);
        EXPECT_EQ(Timestamp::fromFhirDateTime("2022-01-29T11:34:56-01:00"), expectedDateTime);
        // search without time zone:
        EXPECT_EQ(Timestamp::fromFhirSearchDateTime("2022-01-29T12:34:56", Timestamp::UTCTimezone), expectedDateTime);
        EXPECT_EQ(Timestamp::fromFhirSearchDateTime("2022-01-29T12:34:56", Timestamp::GermanTimezone), expectedDateTime - 1h);
    }
    {
        // search without seconds.
        const auto expectedDateTime = Timestamp(sys_days{June/28/2022} + 18h + 54min + 0s);
        EXPECT_EQ(Timestamp::fromFhirSearchDateTime("2022-06-28T18:54Z"), expectedDateTime);
        EXPECT_EQ(Timestamp::fromFhirSearchDateTime("2022-06-28T11:54-07:00"), expectedDateTime);
        // combined with missing time zone:
        EXPECT_EQ(Timestamp::fromFhirSearchDateTime("2022-06-28T18:54", Timestamp::UTCTimezone), expectedDateTime);
        EXPECT_EQ(Timestamp::fromFhirSearchDateTime("2022-06-28T18:54", Timestamp::GermanTimezone),
                  expectedDateTime - 2h);
    }

    {
        // xs:dateTime with sub-second precision
        const auto expectedDateTime = Timestamp(sys_days{January/29/2022} + 12h + 34min + 56s + 123ms);
        EXPECT_EQ(Timestamp::fromFhirDateTime("2022-01-29T12:34:56.123Z", Timestamp::UTCTimezone), expectedDateTime);
    }

    {
        // xs:date
        const auto expectedDateTime = Timestamp(sys_days{January/29/2022});
        EXPECT_EQ(Timestamp::fromFhirDateTime("2022-01-29", Timestamp::UTCTimezone), expectedDateTime);
        EXPECT_EQ(Timestamp::fromFhirDateTime("2022-01-29", Timestamp::GermanTimezone), expectedDateTime - 1h);
    }

    {
        // xs:gYearMonth
        const auto expectedDateTime = Timestamp(sys_days{February/01/2022});
        EXPECT_EQ(Timestamp::fromFhirDateTime("2022-02", Timestamp::UTCTimezone), expectedDateTime);
        EXPECT_EQ(Timestamp::fromFhirDateTime("2022-02", Timestamp::GermanTimezone), expectedDateTime - 1h);
    }

    {
        // xs:gYear
        const auto expectedDateTime = Timestamp(sys_days{January/01/2022});
        EXPECT_EQ(Timestamp::fromFhirDateTime("2022", Timestamp::UTCTimezone), expectedDateTime);
        EXPECT_EQ(Timestamp::fromFhirDateTime("2022", Timestamp::GermanTimezone), expectedDateTime - 1h);
    }
}

TEST_F(TimestampTest, fromFhirSearchDateTime_failForInvalidFormats)//NOLINT(readability-function-cognitive-complexity)
{
    // In some ISO8601 profiles a divider other than T is allowed. Not in ours.
    EXPECT_THROW(Timestamp::fromFhirSearchDateTime("2022-01-29 12:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromFhirSearchDateTime("2022-01-29t12:34:56Z"), ModelException);

    // The std::chrono parser allows fields to omit leading zeros. That is not permitted by FHIR.
    EXPECT_THROW(Timestamp::fromFhirSearchDateTime("2022-1-29T12:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromFhirSearchDateTime("22-01-29T12:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromFhirSearchDateTime("2022-01-2T12:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromFhirSearchDateTime("2022-01-29T2:34:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromFhirSearchDateTime("2022-01-29T12:4:56Z"), ModelException);
    EXPECT_THROW(Timestamp::fromFhirSearchDateTime("2022-01-29T12:34:6Z"), ModelException);
    EXPECT_THROW(Timestamp::fromFhirSearchDateTime("2022-01-29T12:34:56+1:00"), ModelException);
    EXPECT_THROW(Timestamp::fromFhirSearchDateTime("2022-01-29T12:34:56+01:0"), ModelException);

    // Negative year is not permitted.
    EXPECT_THROW(Timestamp::fromFhirSearchDateTime("-2022-01-29T12:34:56Z"), ModelException);

    // A few additional mixed invalid format tests:
    EXPECT_THROW(Timestamp::fromFhirSearchDateTime("2022-01-29T12:34:"), ModelException);
    EXPECT_THROW(Timestamp::fromFhirSearchDateTime("2022-01-29T12:345"), ModelException);
    EXPECT_THROW(Timestamp::fromFhirSearchDateTime("20YY-01-29T12:34"), ModelException);
    EXPECT_THROW(Timestamp::fromFhirSearchDateTime("2022-01-29T12:34 "), ModelException);
}

TEST_F(TimestampTest, fromGermanDate)
{
    auto ts = Timestamp::fromGermanDate("2022-07-12");
    EXPECT_EQ(ts.toXsDateTime(), "2022-07-11T22:00:00.000+00:00");
    EXPECT_EQ(ts.toXsDate(Timestamp::UTCTimezone), "2022-07-11");
    EXPECT_EQ(ts.toXsDate(Timestamp::GermanTimezone), "2022-07-12");
    EXPECT_EQ(ts.toGermanDate(), "2022-07-12");
}

TEST_F(TimestampTest, fromGermanDate2)
{
    auto ts = Timestamp::fromXsDateTime("2022-07-11T22:00:00.000+00:00");
    EXPECT_EQ(ts.toXsDateTime(), "2022-07-11T22:00:00.000+00:00");
    EXPECT_EQ(ts.toXsDate(Timestamp::UTCTimezone), "2022-07-11");
    EXPECT_EQ(ts.toGermanDate(), "2022-07-12");
}

TEST_F(TimestampTest, toDatabaseSUuid)
{
    {
        auto ts = Timestamp::fromXsDateTime("2023-02-15T01:00:00+01:00");
        const auto suuid = ts.toDatabaseSUuid();
        ASSERT_EQ(suuid, "01eb9e8f-9c04-bc00-0000-000000000000");
        ASSERT_EQ(Timestamp::fromDatabaseSUuid(suuid), ts);
    }
    {
        // preserve fractional seconds
        auto ts2 = Timestamp::fromXsDateTime("2023-02-15T01:00:00.222+01:00");
        const auto suuid2 = ts2.toDatabaseSUuid();
        ASSERT_EQ(Timestamp::fromDatabaseSUuid(suuid2), ts2);
    }
    EXPECT_THROW(Timestamp::fromDatabaseSUuid("01eb9e8f-9c04-bc00"), ModelException);
    EXPECT_THROW(Timestamp::fromDatabaseSUuid("g1eb9e8f-9c04-bc00-0000-000000000000"), ModelException);
    EXPECT_THROW(Timestamp::fromDatabaseSUuid("1eb9e8f-9c04-bc00-0000-000000000000"), ModelException);
    EXPECT_THROW(Timestamp::fromDatabaseSUuid("00000000-0000-0000-0000-000000000000"), ModelException);
}
