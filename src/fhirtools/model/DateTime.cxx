/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/model/DateTime.hxx"
#include "fhirtools/FPExpect.hxx"

#include <boost/algorithm/string/split.hpp>
#include <date/tz.h>
#include <charconv>
#include <iomanip>
#include <unordered_set>

namespace fhirtools
{

static constexpr auto* fmtYear = "%Y";
static constexpr auto* fmtYearMonth = "%Y-%m";
static constexpr auto* fmtYearMonthDay = "%F";
static constexpr auto* fmtDateTimeZ = "%FT%H:%M:%SZ";
static constexpr auto* fmtDateTimeTz = "%FT%H:%M:%S%Ez";
static constexpr auto* fmtDateTime = "%FT%H:%M:%S";
static constexpr auto* germanTimezone = "Europe/Berlin";

namespace
{
std::vector<std::string> splitAt(const std::string_view& str, const std::unordered_set<char>& at)
{
    std::vector<std::string> parts;
    boost::split(parts, str, [&at](const char& c) {
        return at.contains(c);
    });
    FPExpect(! parts.empty(), "invalid string: " + std::string{str});
    return parts;
}
std::vector<std::string> splitAt(const std::string_view& str, const char at)
{
    return splitAt(str, std::unordered_set<char>{at});
}
template<typename TInto>
void parsePart(const std::string_view& str, const std::string_view& timeStr, TInto& into)
{
    const auto* endPtr = str.data() + str.size();
    const auto fromCharsResult = std::from_chars(str.data(), endPtr, into);
    FPExpect(fromCharsResult.ptr == endPtr && fromCharsResult.ec == std::errc(),
             "invalid time string: " + std::string{timeStr});
}
}

Time::Time(const std::string& timeStr)
    : mPrecision(Precision::hour)
{
    const auto timeParts = splitAt(timeStr, ':');
    FPExpect(timeParts.size() <= 3, "invalid time string: " + timeStr);
    parseHour(timeParts[0], timeStr);
    if (timeParts.size() >= 2)
    {
        parseMinute(timeParts[1], timeStr);
        mPrecision = Precision::hourMinute;
    }
    if (timeParts.size() == 3)
    {
        parseSecond(timeParts[2], timeStr);
    }
}

Time::Time(const date::hh_mm_ss<std::chrono::milliseconds>& time, Precision precision)
    : mHour{static_cast<Hour>(time.hours().count())}
    , mMinute{static_cast<Minute>(time.minutes().count())}
    , mSecond{static_cast<Second>(time.seconds().count())}
    , mFractions{static_cast<Fractions>(time.subseconds().count())}
    , mPrecision(precision)
{
}

std::string Time::toString(bool full) const
{
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << static_cast<uint32_t>(mHour);
    if (mPrecision > Precision::hour || full)
    {
        oss << ":" << std::setw(2) << std::setfill('0') << static_cast<uint32_t>(mMinute);
    }
    if (mPrecision > Precision::hourMinute || full)
    {
        oss << ":" << std::setw(2) << std::setfill('0') << static_cast<uint32_t>(mSecond);
    }
    if (mPrecision > Precision::hourMinuteSecond || full)
    {
        oss << "." << std::setw(3) << std::setfill('0') << static_cast<uint32_t>(mFractions);
    }
    return oss.str();
}

std::ostream& operator<<(std::ostream& os, const Time& time1)
{
    os << time1.toString();
    return os;
}

void Time::parseHour(const std::string_view& hourStr, const std::string_view& timeStr)
{
    parsePart(hourStr, timeStr, mHour);
    FPExpect(mHour >= 0 && mHour < 24, "invalid time string: " + std::string{timeStr});
}

void Time::parseMinute(const std::string_view& minuteStr, const std::string_view& timeStr)
{
    parsePart(minuteStr, timeStr, mMinute);
    FPExpect(mMinute >= 0 && mMinute < 60, "invalid time string: " + std::string{timeStr});
    mPrecision = Precision::hourMinute;
}

void Time::parseSecond(const std::string_view& secondStr, const std::string_view& timeStr)
{
    const auto secondParts = splitAt(secondStr, '.');
    FPExpect(secondParts.size() <= 2, "invalid time string: " + std::string{timeStr});
    parsePart(secondParts[0], timeStr, mSecond);
    FPExpect(mSecond >= 0 && mSecond < 60, "invalid time string: " + std::string{timeStr});
    mPrecision = Precision::hourMinuteSecond;
    if (secondParts.size() == 2)
    {
        parsePart(secondParts[1], timeStr, mFractions);
        mPrecision = Precision::hourMinuteSecondFraction;
    }
}

bool Time::samePrecision(const Time& other) const
{
    return mPrecision == other.mPrecision;
}

Time::Precision Time::precision() const
{
    return mPrecision;
}

std::optional<std::strong_ordering> Time::compareTo(const Time& other) const
{
    // see http://hl7.org/fhirpath/#datetime-equality
    if (! samePrecision(other))
    {
        return std::nullopt;
    }
    const auto compareHours = mHour <=> other.mHour;
    if (! std::is_eq(compareHours))
    {
        return compareHours;
    }
    const auto compareMinutes = mMinute <=> other.mMinute;
    if (! std::is_eq(compareMinutes))
    {
        return compareMinutes;
    }
    const auto compareSeconds = mSecond <=> other.mSecond;
    if (! std::is_eq(compareSeconds))
    {
        return compareSeconds;
    }
    return mFractions <=> other.mFractions;
}

Date::Date(const std::string& dateStr)
{
    const auto separators = std::count(dateStr.begin(), dateStr.end(), '-');
    FPExpect(separators <= 2, "invalid date string: " + dateStr);
    std::istringstream iss(dateStr);
    if (separators == 0)
    {
        date::from_stream(iss, fmtYear, mYear);
        mPrecision = Precision::year;
    }
    else if (separators == 1)
    {
        date::year_month ym{};
        date::from_stream(iss, fmtYearMonth, ym);
        mYear = ym.year();
        mMonth = ym.month();
        mPrecision = Precision::month;
    }
    else if (separators == 2)
    {
        date::year_month_day ymd{};
        date::from_stream(iss, fmtYearMonthDay, ymd);
        mYear = ymd.year();
        mMonth = ymd.month();
        mDay = ymd.day();
        mPrecision = Precision::day;

    }
    FPExpect(! iss.fail(), "invalid date string: " + dateStr);
    FPExpect(iss.tellg() == static_cast<int64_t>(dateStr.size()), "invalid date string: " + dateStr);
}

Date::Date(const date::year_month_day& ymd, Precision precision)
    : mYear{ymd.year()}
    , mMonth{ymd.month()}
    , mDay{ymd.day()}
    , mPrecision{precision}
{
}

Date::operator date::year_month_day() const
{
    return {mYear, mMonth, mDay};
}

std::string Date::toString(bool full) const
{
    switch (mPrecision)
    {
        case Precision::year: {
            auto s = date::format("%Y", mYear);
            return full ? s + "-01-01" : s;
        }
        case Precision::month: {
            auto s = date::format("%Y-%m", date::year_month(mYear, mMonth));
            return full ? s + "-01" : s;
        }
        case Precision::day: {
            return date::format("%F", date::year_month_day(mYear, mMonth, mDay));
        }
    }
    Fail("Unexpected precision");
}
std::ostream& operator<<(std::ostream& os, const Date& date1)
{
    os << date1.toString();
    return os;
}

std::optional<std::strong_ordering> Date::compareTo(const Date& other) const
{
    if (mYear < other.mYear)
    {
        return std::strong_ordering::less;
    }
    else if (mYear > other.mYear)
    {
        return std::strong_ordering::greater;
    }
    else if (mPrecision == Precision::year && other.mPrecision == Precision::year)
    {
        return std::strong_ordering::equal;
    }
    else if (mPrecision == Precision::year || other.mPrecision == Precision::year)
    {
        return std::nullopt;
    }

    if (mMonth < other.mMonth)
    {
        return std::strong_ordering::less;
    }
    else if (mMonth > other.mMonth)
    {
        return std::strong_ordering::greater;
    }
    else if (mPrecision == Precision::month && other.mPrecision == Precision::month)
    {
        return std::strong_ordering::equal;
    }
    else if (mPrecision == Precision::month || other.mPrecision == Precision::month)
    {
        return std::nullopt;
    }

    if (mDay < other.mDay)
    {
        return std::strong_ordering::less;
    }
    else if (mDay > other.mDay)
    {
        return std::strong_ordering::greater;
    }
    return std::strong_ordering::equal;
}

bool Date::samePrecision(const Date& other) const
{
    return mPrecision == other.mPrecision;
}

Date::Precision Date::precision() const
{
    return mPrecision;
}

DateTime::DateTime(const std::string& dateTimeStr)
{
    auto dateTimeParts = splitAt(dateTimeStr, 'T');
    FPExpect(dateTimeParts.size() <= 2, "invalid DateTime string: " + dateTimeStr);
    mDate = Date(dateTimeParts[0]);
    if (dateTimeParts.size() == 2 && ! dateTimeParts[1].empty())
    {
        const auto timezoneItr = std::ranges::find_first_of(dateTimeParts[1], "Z+-");
        const bool hasTimezone = timezoneItr != dateTimeParts[1].end();
        std::string beforeTimezone{dateTimeParts[1].begin(), timezoneItr};
        if (! beforeTimezone.empty())
        {
            mTime = Time(beforeTimezone);
        }
        if (hasTimezone)
        {
            mTimezone = std::string{timezoneItr, dateTimeParts[1].end()};
        }
    }
    setTimePoint();
}

DateTime::DateTime(const Date& date)
    : mDate(date)
{
    setTimePoint();
}

std::string DateTime::toString(bool full) const
{
    auto str = mDate.toString(full);
    if (mTime)
    {
        str.append("T").append(mTime->toString(full));
    }
    else if (full)
    {
        str.append("T").append(Time{}.toString(true));
    }
    if (mTimezone)
    {
        str.append(*mTimezone);
    }
    return str;
}
std::ostream& operator<<(std::ostream& os, const DateTime& dateTime)
{
    os << dateTime.toString();
    return os;
}

const Date& DateTime::date() const
{
    return mDate;
}

const std::optional<Time>& DateTime::time() const
{
    return mTime;
}

std::optional<std::strong_ordering> DateTime::compareTo(const DateTime& other) const
{
    const auto dateOrdering = mLocalDate.compareTo(other.mLocalDate);
    if (dateOrdering == std::strong_ordering::equal)
    {
        if (mLocalTime.has_value() && other.mLocalTime.has_value())
        {
            return mLocalTime->compareTo(*other.mLocalTime);
        }
        else if (! mLocalTime.has_value() && ! other.mLocalTime.has_value())
        {
            return std::strong_ordering::equal;
        }
        return std::nullopt;
    }
    return dateOrdering;
}

void DateTime::setTimePoint()
{
    const auto& fullStr = toString(true);
    std::istringstream iss(fullStr);
    iss.imbue(std::locale::classic());
    if (mTimezone)
    {
        date::from_stream(iss, (fullStr.back() == 'Z' ? fmtDateTimeZ : fmtDateTimeTz), mTimePoint);
        FPExpect(! iss.fail() && mTimePoint != decltype(mTimePoint){}, "failed to parse DateTime " + fullStr);
    }
    else
    {
        date::local_time<std::chrono::milliseconds> localTime;
        date::from_stream(iss, fmtDateTime, localTime);
        FPExpect(! iss.fail() && localTime != decltype(localTime){}, "failed to parse DateTime " + fullStr);
        mTimePoint = date::make_zoned(germanTimezone, localTime).get_sys_time();
    }
    using zoned_ms = date::zoned_time<std::chrono::milliseconds>;
    auto zt = zoned_ms{germanTimezone, mTimePoint};
    auto localDay = date::floor<date::days>(zt.get_local_time());
    mLocalDate = Date{date::year_month_day{localDay}, mDate.precision()};
    if (mTime) {
        auto timeOfDay = date::hh_mm_ss<std::chrono::milliseconds>{zt.get_local_time() - localDay};
        mLocalTime = Time{timeOfDay, mTime->precision()};
    }
}
}
