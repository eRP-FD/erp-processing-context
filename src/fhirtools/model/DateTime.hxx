/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_MODEL_DATETIME_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_MODEL_DATETIME_HXX

#include <date/date.h>
#include <optional>
#include <ostream>
#include <string>
#include <variant>

namespace fhirtools
{

class Time
{
public:
    using Hour = uint8_t;
    using Minute = uint8_t;
    using Second = uint8_t;
    using Fractions = uint16_t;
    using timepoint_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;
    enum class Precision
    {
        hour = 0,
        hourMinute,
        hourMinuteSecond,
        hourMinuteSecondFraction
    };

    // from http://hl7.org/fhirpath/#time, without "@T"
    // opposing to xs:time, the literal also supports incomplete timestamps such as 12:00 or 12
    explicit Time(const std::string& timeStr);
    explicit Time(const date::hh_mm_ss<std::chrono::milliseconds>& time, Precision precision);

    [[nodiscard]] std::string toString(bool full = false) const;
    friend std::ostream& operator<<(std::ostream& os, const Time& time1);

    Time() = default;
    Time(const Time&) = default;
    Time(Time&&) = default;
    Time& operator=(const Time&) = default;
    Time& operator=(Time&&) = default;

    [[nodiscard]] std::optional<std::strong_ordering> compareTo(const Time& other) const;
    [[nodiscard]] bool samePrecision(const Time& other) const;
    Precision precision() const;

private:
    void parseHour(const std::string_view& hourStr, const std::string_view& timeStr);
    void parseMinute(const std::string_view& minuteStr, const std::string_view& timeStr);
    void parseSecond(const std::string_view& secondStr, const std::string_view& timeStr);

    Hour mHour{0};
    Minute mMinute{0};
    Second mSecond{0};
    Fractions mFractions{0};
    Precision mPrecision{};
};

class Date
{
public:
    enum class Precision {
        year = 0,
        month,
        day
    };
    explicit Date(const std::string& dateStr);
    explicit Date(const date::year_month_day& ymd, Precision precision);

    explicit operator date::year_month_day() const;

    [[nodiscard]] std::string toString(bool full = false) const;
    friend std::ostream& operator<<(std::ostream& os, const Date& date1);


    Date() = default;
    Date(const Date&) = default;
    Date(Date&&) = default;
    Date& operator=(const Date&) = default;
    Date& operator=(Date&&) = default;

    [[nodiscard]] std::optional<std::strong_ordering> compareTo(const Date& other) const;
    [[nodiscard]] bool samePrecision(const Date& other) const;
    Precision precision() const;

private:
    date::year mYear{};
    date::month mMonth{};
    date::day mDay{};
    Precision mPrecision{Precision::year};
};

class DateTime
{
public:
    // from http://hl7.org/fhirpath/#datetime, without "@"
    // opposing to xs:dateTime, the literal also supports incomplete timestamps such as 12:00 or 12
    explicit DateTime(const std::string& dateTimeStr);
    explicit DateTime(const Date& date);

    [[nodiscard]] std::string toString(bool full = false) const;
    friend std::ostream& operator<<(std::ostream& os, const DateTime& dateTime);

    [[nodiscard]] const Date& date() const;
    [[nodiscard]] const std::optional<Time>& time() const;

    DateTime() = default;
    DateTime(const DateTime&) = default;
    DateTime(DateTime&&) = default;
    DateTime& operator=(const DateTime&) = default;
    DateTime& operator=(DateTime&&) = default;

    [[nodiscard]] std::optional<std::strong_ordering> compareTo(const DateTime& other) const;

private:
    using timestamp_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;

    void setTimePoint();

    Date mDate{};
    Date mLocalDate{};
    std::optional<Time> mTime{};
    std::optional<Time> mLocalTime{};
    std::optional<std::string> mTimezone{};
    timestamp_t mTimePoint;
};
}

#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_MODEL_DATETIME_HXX
