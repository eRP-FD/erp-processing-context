#include "fhirtools/model/PrimitiveValue.hxx"
#include "fhirtools/validator/internal/CodeChecker.hxx"
#include "fhirtools/validator/ValidationResult.hxx"

#include <gtest/gtest.h>

using fhirtools::Date;
using fhirtools::DateTime;
using fhirtools::DecimalType;
using fhirtools::PrimitiveValue;
using fhirtools::QuantityType;
using fhirtools::Time;

class PrimitiveTypeTest : public testing::Test
{
public:
    void testCode(std::string_view code, std::optional<std::string_view> expectedMessage = std::nullopt)
    {
        auto result = fhirtools::CodeChecker::checkCode(code, "test");
        if (expectedMessage.has_value())
        {
            const auto& resultSet = result.results();
            ASSERT_EQ(resultSet.size(), 1);
            ASSERT_EQ(to_string(*resultSet.begin()), expectedMessage);
        }
        else
        {
            ASSERT_LE(result.highestSeverity(), fhirtools::Severity::warning) << (result.dumpToLog(), "see dump");
        }
    }
};

// http://hl7.org/fhir/R4/datatypes.html#primitive
// Technically, a code is restricted to a string which has at least one character and no leading
// or trailing whitespace, and where there is no whitespace other than single spaces in the contents
TEST_F(PrimitiveTypeTest, code_fail)
{
    // clang-format off
    EXPECT_NO_FATAL_FAILURE(testCode(" leading"      , "test: error: code must not start with whitespace."));
    EXPECT_NO_FATAL_FAILURE(testCode("trailing "     , "test: error: code must not end on whitespace."));
    EXPECT_NO_FATAL_FAILURE(testCode("Double  Space" , "test: error: code must not have more than one consecutive space."));
    EXPECT_NO_FATAL_FAILURE(testCode("Tab\tCharacter", "test: error: code must not contain whitespace other than space."));
    EXPECT_NO_FATAL_FAILURE(testCode("New\nLine"     , "test: error: code must not contain whitespace other than space."));
    EXPECT_NO_FATAL_FAILURE(testCode(" "             , "test: error: code must not start with whitespace."));

    EXPECT_NO_FATAL_FAILURE(testCode("\xe5What!"     , "test: error: invalid utf-8 in code."));
#define NBSP "\xC2\xA0"
    EXPECT_NO_FATAL_FAILURE(testCode("B" NBSP "E"    , "test: error: code must not contain whitespace other than space."));
#define EnSpace "\xE2\x80\x82"
    EXPECT_NO_FATAL_FAILURE(testCode("B" EnSpace "E" , "test: error: code must not contain whitespace other than space."));

}

TEST_F(PrimitiveTypeTest, code_valid)
{
    // clang-format off
    EXPECT_NO_FATAL_FAILURE(testCode("A"));                         // Minimum length
    EXPECT_NO_FATAL_FAILURE(testCode("Code\x20With\x20Space"));     // Single spaces (\x20)

    // UTF-8: "München" (ü = \xc3\xbc)
    EXPECT_NO_FATAL_FAILURE(testCode("M\xc3\xbcnchen"));

    // UTF-8: "你好" (Nǐ hǎo)
    EXPECT_NO_FATAL_FAILURE(testCode("\xe4\xbd\xa0\xe5\xa5\xbd"));    // clang-format on

}

// http://hl7.org/fhirpath/N1/#integer-conversion-functions
// toInteger(): Integer, String matching (\+|-)?\d+, Boolean (true -> 1, false -> 0); everything else is not convertible
TEST(PrimitiveValueTest, asInt)
{
    EXPECT_EQ(PrimitiveValue{int32_t{12}}.asInt(), 12);
    EXPECT_EQ(PrimitiveValue{int32_t{-7}}.asInt(), -7);
    EXPECT_EQ(PrimitiveValue{std::string{"12"}}.asInt(), 12);
    EXPECT_EQ(PrimitiveValue{std::string{"-12"}}.asInt(), -12);
    EXPECT_EQ(PrimitiveValue{std::string{"+12"}}.asInt(), 12);
    EXPECT_EQ(PrimitiveValue{std::string{"0"}}.asInt(), 0);
    EXPECT_EQ(PrimitiveValue{true}.asInt(), 1);
    EXPECT_EQ(PrimitiveValue{false}.asInt(), 0);

    EXPECT_THROW((void) PrimitiveValue{std::string{"abc"}}.asInt(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{std::string{"3.14"}}.asInt(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{std::string{"1 "}}.asInt(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{std::string{"+"}}.asInt(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{std::string{""}}.asInt(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{DecimalType{"1.0"}}.asInt(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{Date{"2022-04-29"}}.asInt(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{Time{"11:47:30"}}.asInt(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{DateTime{"2022-04-29T11:47:30"}}.asInt(), std::exception);
    EXPECT_THROW(((void) PrimitiveValue{QuantityType{10, "km"}}.asInt()), std::exception);
}

// http://hl7.org/fhirpath/N1/#decimal-conversion-functions
// toDecimal(): Integer, Decimal, String matching (\+|-)?\d+(\.\d+)?, Boolean (true -> 1.0, false -> 0.0)
TEST(PrimitiveValueTest, asDecimal)
{
    EXPECT_EQ(PrimitiveValue{DecimalType{"1.2"}}.asDecimal(), DecimalType{"1.2"});
    EXPECT_EQ(PrimitiveValue{DecimalType{"-3.14"}}.asDecimal(), DecimalType{"-3.14"});
    EXPECT_EQ(PrimitiveValue{int32_t{12}}.asDecimal(), DecimalType{"12.0"});
    EXPECT_EQ(PrimitiveValue{int32_t{-5}}.asDecimal(), DecimalType{"-5.0"});
    EXPECT_EQ(PrimitiveValue{std::string{"3.14"}}.asDecimal(), DecimalType{"3.14"});
    EXPECT_EQ(PrimitiveValue{std::string{"32"}}.asDecimal(), DecimalType{"32.0"});
    EXPECT_EQ(PrimitiveValue{std::string{"-0.5"}}.asDecimal(), DecimalType{"-0.5"});
    EXPECT_EQ(PrimitiveValue{std::string{"+0.5"}}.asDecimal(), DecimalType{"0.5"});
    EXPECT_EQ(PrimitiveValue{true}.asDecimal(), DecimalType{"1.0"});
    EXPECT_EQ(PrimitiveValue{false}.asDecimal(), DecimalType{"0.0"});
    EXPECT_EQ(PrimitiveValue{std::string{"1."}}.asDecimal(), DecimalType{"1."});
    EXPECT_EQ(PrimitiveValue{std::string{".5"}}.asDecimal(), DecimalType{".5"});
    EXPECT_EQ(PrimitiveValue{std::string{"1e3"}}.asDecimal(), DecimalType{"1e3"});

    EXPECT_THROW((void) PrimitiveValue{std::string{"abc"}}.asDecimal(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{std::string{""}}.asDecimal(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{Date{"2022-04-29"}}.asDecimal(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{Time{"11:47:30"}}.asDecimal(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{DateTime{"2022-04-29T11:47:30"}}.asDecimal(), std::exception);
    EXPECT_THROW(((void) PrimitiveValue{QuantityType{10, "km"}}.asDecimal()), std::exception);
}

// http://hl7.org/fhirpath/N1/#boolean-conversion-functions
// toBoolean(): Boolean; String 'true','t','yes','y','1','1.0' / 'false','f','no','n','0','0.0' (case ignored);
// Integer 1 / 0; Decimal 1.0 / 0.0; everything else is not convertible
TEST(PrimitiveValueTest, asBool)
{
    EXPECT_TRUE(PrimitiveValue{true}.asBool());
    EXPECT_FALSE(PrimitiveValue{false}.asBool());
    for (const char* s : {"true", "TRUE", "True", "t", "T", "yes", "YES", "y", "Y", "1", "1.0"})
    {
        EXPECT_NO_THROW(
            EXPECT_TRUE(PrimitiveValue{std::string{s}}.asBool()) << s;
        ) << s;
    }
    for (const char* s : {"false", "FALSE", "False", "f", "F", "no", "NO", "n", "N", "0", "0.0"})
    {
        EXPECT_FALSE(PrimitiveValue{std::string{s}}.asBool()) << s;
    }
    EXPECT_THROW((void) PrimitiveValue{std::string{"maybe"}}.asBool(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{std::string{"2"}}.asBool(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{std::string{""}}.asBool(), std::exception);

    EXPECT_TRUE(PrimitiveValue{int32_t{1}}.asBool());
    EXPECT_FALSE(PrimitiveValue{int32_t{0}}.asBool());
    EXPECT_THROW((void) PrimitiveValue{int32_t{12}}.asBool(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{int32_t{-1}}.asBool(), std::exception);

    EXPECT_TRUE(PrimitiveValue{DecimalType{"1.0"}}.asBool());
    EXPECT_FALSE(PrimitiveValue{DecimalType{"0.0"}}.asBool());
    EXPECT_THROW((void) PrimitiveValue{DecimalType{"1.2"}}.asBool(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{DecimalType{"2.0"}}.asBool(), std::exception);

    EXPECT_THROW((void) PrimitiveValue{Date{"2022-04-29"}}.asBool(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{Time{"11:47:30"}}.asBool(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{DateTime{"2022-04-29T11:47:30"}}.asBool(), std::exception);
    EXPECT_THROW(((void) PrimitiveValue{QuantityType{1, "1"}}.asBool()), std::exception);
}

// http://hl7.org/fhirpath/N1/#tostring-string
// Boolean: true/false; Integer: (\+|-)?\d+; Decimal: (\+|-)?\d+(.\d+)?; Quantity: (\+|-)?\d+(.\d+)? '.*';
// Date: YYYY-MM-DD; DateTime: YYYY-MM-DDThh:mm:ss.fff(+|-)hh:mm; Time: hh:mm:ss.fff(+|-)hh:mm;
// partial dates and times are only specified to the level of precision of the value
TEST(PrimitiveValueTest, asString)
{
    EXPECT_EQ(PrimitiveValue{std::string{"value"}}.asString(), "value");
    EXPECT_EQ(PrimitiveValue{std::string{}}.asString(), "");
    EXPECT_EQ(PrimitiveValue{true}.asString(), "true");
    EXPECT_EQ(PrimitiveValue{false}.asString(), "false");
    EXPECT_EQ(PrimitiveValue{int32_t{12}}.asString(), "12");
    EXPECT_EQ(PrimitiveValue{int32_t{-12}}.asString(), "-12");
    EXPECT_EQ(PrimitiveValue{int32_t{0}}.asString(), "0");
    EXPECT_EQ(PrimitiveValue{DecimalType{"3.14"}}.asString(), "3.14");
    EXPECT_EQ(PrimitiveValue{DecimalType{"-1.2"}}.asString(), "-1.2");
    EXPECT_EQ(PrimitiveValue{Date{"2022-04-29"}}.asString(), "2022-04-29");
    EXPECT_EQ(PrimitiveValue{Date{"2022-04"}}.asString(), "2022-04");
    EXPECT_EQ(PrimitiveValue{Date{"2022"}}.asString(), "2022");
    EXPECT_EQ(PrimitiveValue{Time{"11:47:30"}}.asString(), "11:47:30");
    EXPECT_EQ(PrimitiveValue{Time{"11:47"}}.asString(), "11:47");
    EXPECT_EQ(PrimitiveValue{Time{"11"}}.asString(), "11");
    EXPECT_EQ(PrimitiveValue{Time{"11:47:30.123"}}.asString(), "11:47:30.123");
    EXPECT_EQ(PrimitiveValue{DateTime{"2022-04-29T11:47:30"}}.asString(), "2022-04-29T11:47:30");
    EXPECT_EQ(PrimitiveValue{DateTime{"2022-04-29T11:47:30.123+02:00"}}.asString(), "2022-04-29T11:47:30.123+02:00");
    EXPECT_EQ(PrimitiveValue{DateTime{"2022-04"}}.asString(), "2022-04");
}

TEST(PrimitiveValueTest, DISABLED_Erp39305_asString)
{
    EXPECT_EQ((PrimitiveValue{QuantityType{DecimalType{"10.0"}, "km"}}.asString()), "10.0 'km'");
    EXPECT_EQ((PrimitiveValue{QuantityType{DecimalType{"-2.5"}, "mg"}}.asString()), "-2.5 'mg'");
}

// http://hl7.org/fhirpath/N1/#todate-date
// toDate(): Date, DateTime, String in format YYYY-MM-DD; partial dates (e.g. '2012-01') result in a partial date
TEST(PrimitiveValueTest, asDate)
{
    EXPECT_EQ(PrimitiveValue{Date{"2022-04-29"}}.asDate().compareTo(Date{"2022-04-29"}), std::strong_ordering::equal);
    EXPECT_EQ(PrimitiveValue{Date{"2012-01"}}.asDate().precision(), Date::Precision::month);
    EXPECT_EQ(PrimitiveValue{Date{"2012"}}.asDate().precision(), Date::Precision::year);

    EXPECT_EQ(PrimitiveValue{DateTime{"2022-04-29T11:47:30"}}.asDate().compareTo(Date{"2022-04-29"}),
              std::strong_ordering::equal);
    EXPECT_EQ(PrimitiveValue{DateTime{"2022-04-29T11:47:30"}}.asDate().precision(), Date::Precision::day);

    EXPECT_EQ(PrimitiveValue{std::string{"2022-04-29"}}.asDate().compareTo(Date{"2022-04-29"}),
              std::strong_ordering::equal);
    EXPECT_EQ(PrimitiveValue{std::string{"2012-01"}}.asDate().precision(), Date::Precision::month);
    EXPECT_EQ(PrimitiveValue{std::string{"2012"}}.asDate().precision(), Date::Precision::year);

    EXPECT_THROW((void) PrimitiveValue{std::string{"not a date"}}.asDate(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{std::string{"2022-13-45"}}.asDate(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{std::string{""}}.asDate(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{int32_t{12}}.asDate(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{DecimalType{"1.2"}}.asDate(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{true}.asDate(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{Time{"11:47:30"}}.asDate(), std::exception);
    EXPECT_THROW(((void) PrimitiveValue{QuantityType{10, "km"}}.asDate()), std::exception);
}

// http://hl7.org/fhirpath/N1/#time-conversion-functions
// toTime(): Time, String in format hh:mm:ss.fff(+|-)hh:mm; partial times (e.g. '10:00') result in a partial time
TEST(PrimitiveValueTest, asTime)
{
    EXPECT_EQ(PrimitiveValue{Time{"11:47:30"}}.asTime().compareTo(Time{"11:47:30"}), std::strong_ordering::equal);
    EXPECT_EQ(PrimitiveValue{Time{"10:00"}}.asTime().precision(), Time::Precision::hourMinute);

    EXPECT_EQ(PrimitiveValue{std::string{"11:47:30"}}.asTime().compareTo(Time{"11:47:30"}),
              std::strong_ordering::equal);
    EXPECT_EQ(PrimitiveValue{std::string{"10:00"}}.asTime().precision(), Time::Precision::hourMinute);
    EXPECT_EQ(PrimitiveValue{std::string{"10"}}.asTime().precision(), Time::Precision::hour);
    EXPECT_EQ(PrimitiveValue{std::string{"11:47:30"}}.asTime().precision(), Time::Precision::hourMinuteSecond);
    EXPECT_EQ(PrimitiveValue{std::string{"11:47:30.123"}}.asTime().precision(),
              Time::Precision::hourMinuteSecondFraction);

    EXPECT_THROW((void) PrimitiveValue{std::string{"not a time"}}.asTime(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{std::string{"25:00:00"}}.asTime(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{std::string{""}}.asTime(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{int32_t{12}}.asTime(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{DecimalType{"1.2"}}.asTime(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{true}.asTime(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{Date{"2022-04-29"}}.asTime(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{DateTime{"2022-04-29T11:47:30"}}.asTime(), std::exception);
    EXPECT_THROW(((void) PrimitiveValue{QuantityType{10, "km"}}.asTime()), std::exception);
}

// http://hl7.org/fhirpath/N1/#datetime-conversion-functions
// toDateTime(): DateTime; Date (year, month, day of the Date, time components empty, not zero);
// String in format YYYY-MM-DDThh:mm:ss.fff(+|-)hh:mm; partial datetimes result in a partial datetime
TEST(PrimitiveValueTest, asDateTime)
{
    EXPECT_EQ(PrimitiveValue{DateTime{"2022-04-29T11:47:30"}}.asDateTime().compareTo(DateTime{"2022-04-29T11:47:30"}),
              std::strong_ordering::equal);

    const auto fromDate = PrimitiveValue{Date{"2022-04-29"}}.asDateTime();
    EXPECT_FALSE(fromDate.time().has_value());
    EXPECT_EQ(fromDate.date().compareTo(Date{"2022-04-29"}), std::strong_ordering::equal);
    EXPECT_EQ(fromDate.toString(), "2022-04-29");
    EXPECT_EQ(PrimitiveValue{Date{"2022-04"}}.asDateTime().date().precision(), Date::Precision::month);
    EXPECT_FALSE(PrimitiveValue{Date{"2022-04"}}.asDateTime().time().has_value());

    EXPECT_EQ(PrimitiveValue{std::string{"2022-04-29T11:47:30"}}
                  .asDateTime()
                  .compareTo(DateTime{"2022-04-29T11:47:30"}),
              std::strong_ordering::equal);
    EXPECT_EQ(PrimitiveValue{std::string{"2022-04-29T11:47:30+02:00"}}
                  .asDateTime()
                  .compareTo(DateTime{"2022-04-29T09:47:30Z"}),
              std::strong_ordering::equal);
    const auto partial = PrimitiveValue{std::string{"2012-01-01T10:00"}}.asDateTime();
    ASSERT_TRUE(partial.time().has_value());
    EXPECT_EQ(partial.time()->precision(), Time::Precision::hourMinute);
    EXPECT_EQ(partial.toString(), "2012-01-01T10:00");

    EXPECT_THROW((void) PrimitiveValue{std::string{"not a datetime"}}.asDateTime(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{std::string{""}}.asDateTime(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{int32_t{12}}.asDateTime(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{DecimalType{"1.2"}}.asDateTime(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{true}.asDateTime(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{Time{"11:47:30"}}.asDateTime(), std::exception);
    EXPECT_THROW(((void) PrimitiveValue{QuantityType{10, "km"}}.asDateTime()), std::exception);
}

// http://hl7.org/fhirpath/N1/#toquantityunit-string-quantity
// toQuantity(): Integer/Decimal (default unit '1'), Quantity, Boolean (true -> 1.0 '1', false -> 0.0 '1'),
// String matching (?'value'(\+|-)?\d+(\.\d+)?)\s*('(?'unit'[^']+)'|(?'time'[a-zA-Z]+))?
TEST(PrimitiveValueTest, asQuantity)
{
    const QuantityType km{10, "km"};
    EXPECT_EQ(PrimitiveValue{km}.asQuantity().compareTo(km), std::strong_ordering::equal);
    EXPECT_EQ(PrimitiveValue{km}.asQuantity().unit(), "km");

    EXPECT_THROW((void) PrimitiveValue{std::string{"abc"}}.asQuantity(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{std::string{""}}.asQuantity(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{Date{"2022-04-29"}}.asQuantity(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{Time{"11:47:30"}}.asQuantity(), std::exception);
    EXPECT_THROW((void) PrimitiveValue{DateTime{"2022-04-29T11:47:30"}}.asQuantity(), std::exception);
}

TEST(PrimitiveValueTest, DISABLED_Erp39305_asQuantity)
{
    const auto fromTrue = PrimitiveValue{true}.asQuantity();
    EXPECT_EQ(fromTrue.value(), DecimalType{"1.0"});
    EXPECT_EQ(fromTrue.unit(), "1");
    const auto fromFalse = PrimitiveValue{false}.asQuantity();
    EXPECT_EQ(fromFalse.value(), DecimalType{"0.0"});
    EXPECT_EQ(fromFalse.unit(), "1");

    const auto fromInt = PrimitiveValue{int32_t{12}}.asQuantity();
    EXPECT_EQ(fromInt.value(), DecimalType{"12.0"});
    EXPECT_EQ(fromInt.unit(), "1");

    const auto fromDecimal = PrimitiveValue{DecimalType{"1.2"}}.asQuantity();
    EXPECT_EQ(fromDecimal.value(), DecimalType{"1.2"});
    EXPECT_EQ(fromDecimal.unit(), "1");

    // calendar durations are converted to seconds (4 * 24 * 60 * 60)
    const auto days = PrimitiveValue{std::string{"4 days"}}.asQuantity();
    EXPECT_EQ(days.value(), DecimalType{"345600"});
    EXPECT_EQ(days.unit(), "s");

    const auto mmHg = PrimitiveValue{std::string{"10 'mg[Hg]'"}}.asQuantity();
    EXPECT_EQ(mmHg.value(), DecimalType{"10.0"});
    EXPECT_EQ(mmHg.unit(), "mg[Hg]");
    const auto noSpace = PrimitiveValue{std::string{"-2.5'mg'"}}.asQuantity();
    EXPECT_EQ(noSpace.value(), DecimalType{"-2.5"});
    EXPECT_EQ(noSpace.unit(), "mg");

    const auto noUnit = PrimitiveValue{std::string{"3.14"}}.asQuantity();
    EXPECT_EQ(noUnit.value(), DecimalType{"3.14"});
    EXPECT_EQ(noUnit.unit(), "1");
}

// http://hl7.org/fhirpath/N1/#toquantityunit-string-quantity
// Calendar duration conversion factors:
// 1 year = 12 months or 365 days, 1 month = 30 days, 1 day = 24 hours, 1 hour = 60 minutes,
// 1 minute = 60 seconds, 1 second = 1 's'
TEST(PrimitiveValueTest, DISABLED_Erp39305_asQuantityCalendarDurations)
{
    struct TestCase
    {
        const char* input;
        const char* seconds;
    };
    // clang-format off
    const std::vector<TestCase> testCases = {
        {"1 second" , "1"},
        {"1 seconds", "1"},
        {"1 minute" , "60"},
        {"1 minutes", "60"},
        {"1 hour"   , "3600"},
        {"1 hours"  , "3600"},
        {"1 day"    , "86400"},
        {"1 days"   , "86400"},
        {"1 month"  , "2592000"},
        {"1 months" , "2592000"},
        {"1 year"   , "31536000"},
        {"1 years"  , "31536000"},
        {"2 days"   , "172800"},
        {"0.5 hours", "1800"},
        {"12 months", "31104000"},
        {"-1 minute", "-60"},
    };
    // clang-format on
    for (const auto& testCase : testCases)
    {
        const auto quantity = PrimitiveValue{std::string{testCase.input}}.asQuantity();
        EXPECT_EQ(quantity.value(), DecimalType{testCase.seconds}) << testCase.input;
        EXPECT_EQ(quantity.unit(), "s") << testCase.input;
    }
    const auto seconds = PrimitiveValue{std::string{"5 's'"}}.asQuantity();
    EXPECT_EQ(seconds.value(), DecimalType{"5"});
    EXPECT_EQ(seconds.unit(), "s");
}
