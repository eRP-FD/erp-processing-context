/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/Time.hxx"
#include "library/json/JsonWriter.hxx"
#include "library/util/Assert.hxx"

#include <boost/regex.hpp>
#include <date/tz.h>
#include <gsl/gsl-lite.hpp>
#include <array>
#include <cmath>
#include <iomanip>
#include <optional>

#include "shared/util/String.hxx"

#ifdef _WIN32
// For struct timeval https://docs.microsoft.com/en-us/windows/win32/api/winsock2/ns-winsock2-timeval
#include <winsock2.h>
#endif

namespace epa
{

#define AssertTimeFormat(expression)                                                               \
    AssertForResponse(expression) << assertion::exceptionType<TimeFormatException>()

#define HAS_TIMEZONE_SUPPORT __cpp_lib_chrono >= 201907L

namespace
{
    std::optional<SystemTime> parse(const std::string& format, const std::string& time)
    {
        std::istringstream stream{time};
        stream.imbue(std::locale::classic());

#if HAS_TIMEZONE_SUPPORT
        std::chrono::utc_clock::time_point result;
        if (stream >> std::chrono::parse(format, result))
            return std::chrono::utc_clock::to_sys(result);
#else
        SystemTime result;
        if (stream >> date::parse(format, result))
            return result;
#endif

        return std::nullopt;
    }

    /**
     * Allows to get the timezone offset for the local time zone in seconds.
     */
    int64_t getTimezoneOffsetInSeconds()
    {
        const std::time_t now = time(nullptr);
        struct tm tmLocal
        {
        };
        struct tm tmGm
        {
        };
#if _POSIX_C_SOURCE >= 1 || _XOPEN_SOURCE || _BSD_SOURCE || _SVID_SOURCE || _POSIX_SOURCE
        Assert(localtime_r(&now, &tmLocal));
        Assert(gmtime_r(&now, &tmGm));
#elif defined _WIN32
        _localtime64_s(&tmLocal, &now);
        _gmtime64_s(&tmGm, &now);
#else
        tmLocal = *localtime(&now);
        tmGm = *gmtime(&now);
#endif
        return mktime(&tmLocal) - mktime(&tmGm);
    }
}


int Time::toDaysCount(date::year_month_day ymd)
{
    return date::sys_days(ymd).time_since_epoch().count();
}


date::year_month_day Time::fromDaysCount(const int countOfDays)
{
    return date::year_month_day(date::sys_days(date::days(countOfDays)));
}


std::string Time::toIso8601DateAndTimeBasic(SystemTime time)
{
    auto seconds = date::floor<std::chrono::seconds>(time);
    return date::format(std::locale::classic(), "%Y%m%dT%H%M%SZ", seconds);
}


std::string Time::toIso8601DateBasic(SystemTime time)
{
    auto seconds = date::floor<std::chrono::seconds>(time);
    return date::format(std::locale::classic(), "%Y%m%d", seconds);
}


std::string Time::toIso8601Date(date::year_month_day date)
{
    return date::format(std::locale::classic(), "%Y-%m-%d", date);
}


std::string Time::toIso8601DateAndTimeUtc(const SystemTime time)
{
    // Unfortunately we cannot specify the precision date::format() shall produce. It may be without
    // sub seconds (if the given time point is seconds aligned) or it may be microseconds or
    // nanoseconds depending on the implementation. Therefore, we force seconds precision and append
    // the microseconds ourselves.
    const auto seconds = date::floor<std::chrono::seconds>(time);
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(time - seconds);

    std::ostringstream s;
    s << date::format(std::locale::classic(), "%Y-%m-%dT%H:%M:%S.", seconds) << std::setfill('0')
      << std::setw(6) << micros.count();
    return s.str();
}


date::year_month_day Time::fromIso8601Date(const std::string& iso8601Date)
{
    std::istringstream stream{iso8601Date};
    stream.imbue(std::locale::classic());

    static const boost::regex dateAndOptionalTz(
        R"((-)?([0-9]{4})-([0-1][0-9])-([0-3][0-9])((Z|(\+|-)([0-1][0-9]):([0-5][0-9])))?)");
    boost::smatch matchResult;
    AssertTimeFormat(boost::regex_match(iso8601Date, matchResult, dateAndOptionalTz))
        << "date '" << logging::sensitive(iso8601Date) << "' is not in the required format";

    AssertTimeFormat(matchResult.size() == 10)
        << "date '" << logging::sensitive(iso8601Date) << "' match against regex failed";

    const auto year = stoi(matchResult[2]);
    date::year_month_day result(
        date::year(matchResult[1].matched ? -year : year),
        date::month(static_cast<unsigned>(stoi(matchResult[3]))),
        date::day(static_cast<unsigned>(stoi(matchResult[4]))));

    AssertTimeFormat(result.ok()) << "date '" << logging::sensitive(iso8601Date)
                                  << "' is syntactically but not semantically correct";

    if (matchResult[5].matched)
    {
        // There is a time zone.
        if (matchResult[5] != "Z")
        {
            // It is not 'Z'. Expect an explicit hour:minute offset.
            AssertTimeFormat(
                matchResult[7].matched && matchResult[8].matched && matchResult[9].matched)
                << "invalid time zone in date '" << logging::sensitive(iso8601Date) << "'";

            // Time zone has three elements, sign, hours, minutes.
            AssertTimeFormat(stoi(matchResult[8]) <= 14)
                << "invalid hour in time zone in date '" << logging::sensitive(iso8601Date) << "'";
            AssertTimeFormat(stoi(matchResult[9]) < 60) << "invalid minutes in time zone in date '"
                                                        << logging::sensitive(iso8601Date) << "'";
            AssertTimeFormat(matchResult[8] != "14" || matchResult[9] == "00")
                << "invalid time zone in date '" << logging::sensitive(iso8601Date) << "'";
        }
    }

    return result;
}


date::year_month_day Time::fromInternationalDate(const std::string& internationalDate)
{
    static const boost::regex pattern("[0-9]{4}-[0-9]{2}-[0-9]{2}");

    boost::smatch matches;
    AssertTimeFormat(boost::regex_match(internationalDate, matches, pattern))
        << "Invalid international date: '" << logging::sensitive(internationalDate)
        << "' (expected format: YYYY-MM-DD)";

    return fromIso8601Date(internationalDate);
}


SystemTime Time::fromIso8601DateAndTimeUtc(const std::string& iso8601DateAndTime)
{
    std::optional<SystemTime> result = parse("%Y-%m-%dT%H:%M:%S", iso8601DateAndTime);
    AssertTimeFormat(result.has_value())
        << "Invalid ISO8601 date and time: " << logging::sensitive(iso8601DateAndTime);
    return *result;
}


std::string Time::toRfc3339(SystemTime time)
{
    // First, reduce to microseconds precision, otherwise the algorithm below may falsely take
    // microseconds instead of milliseconds or seconds - when there are nanoseconds.
    const auto microseconds = std::chrono::floor<std::chrono::microseconds>(time);
    time = microseconds;

    // Do not print sub-second precision if our timestamp is a full second.
    const auto seconds = std::chrono::floor<std::chrono::seconds>(time);
    if (seconds == time)
        return date::format(std::locale::classic(), "%Y-%m-%dT%H:%M:%SZ", seconds);

    // Do not print sub-millisecond precision if our timestamp does not have it.
    const auto milliseconds = std::chrono::floor<std::chrono::milliseconds>(time);
    if (milliseconds == time)
        return date::format(std::locale::classic(), "%Y-%m-%dT%H:%M:%SZ", milliseconds);

    // Equivalent to %FT%TZ, but let's use the more explicit version for consistency with toDtm.
    return date::format(std::locale::classic(), "%Y-%m-%dT%H:%M:%SZ", microseconds);
}


SystemTime Time::fromRfc3339(const std::string& rfc3339)
{
    std::optional<SystemTime> result;

    result = parse("%Y-%m-%dT%H:%M:%SZ", rfc3339);
    if (result.has_value())
        return result.value();

    // Let's try again with %Ez (UTC offset in HH:MM), instead of Z (UTC).
    result = parse("%Y-%m-%dT%H:%M:%S%Ez", rfc3339);

    AssertTimeFormat(result.has_value()) << "Invalid RFC3339 date: " << logging::sensitive(rfc3339);

    // Note that this implementation does not lower-case T/Z characters, both of which are optional
    // features according to RFC3339.

    return result.value();
}


std::string Time::toDtm(SystemTime time)
{
    const auto seconds = date::floor<std::chrono::seconds>(time);
    return date::format(std::locale::classic(), "%Y%m%d%H%M%S", seconds);
}


// GEMREQ-start A_14760-20#fromDtm
SystemTime Time::fromDtm(std::string dtm)
{
    // Expected length is 14 (yyyymmddHHMMSS), at least yyyy must be given.
    AssertTimeFormat(dtm.length() >= 4 && dtm.length() <= 14 && dtm.length() % 2 == 0)
        << "Invalid DTM date: " << logging::sensitive(dtm);

    // Fill up month and day (if mmissing) with "01"
    if (dtm.length() == 4)
        dtm += "01";
    if (dtm.length() == 6)
        dtm += "01";

    // Missing fields after the date are implied to be zeroes; this is our own interpretation.
    // Technically, even "2001" is a valid DTM year. However, we do not know how to convert this to
    // a time_point.
    // If we ever need to round-trip DTM dates with a precision of less than a second without losing
    // precision information, i.e. toDtm(fromDtm("2001")) == "2001", we might need to use a
    // more flexible data type than std::chrono::time_point.
    dtm.append(14 - dtm.length(), '0');

    std::optional<SystemTime> result = parse("%Y%m%d%H%M%S", dtm);
    AssertTimeFormat(result.has_value())
        << "Invalid DTM date even after adjustment: " << logging::sensitive(dtm);
    return result.value();
}
// GEMREQ-end A_14760-20#fromDtm


SystemTime Time::fromSecondsSinceEpoch(const std::int64_t seconds)
{
    return std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>(
        std::chrono::duration<std::int64_t>(seconds));
}


SystemTime Time::fromDoubleSecondsSinceEpoch(double seconds)
{
    const auto& num = std::chrono::system_clock::period::num;
    const auto& den = std::chrono::system_clock::period::den;
    const auto doubleTicks = std::round(seconds * den / num);
    const auto integerTicks = gsl::narrow<std::chrono::system_clock::rep>(doubleTicks);
    return SystemTime(std::chrono::system_clock::duration(integerTicks));
}


double Time::toDoubleSecondsSinceEpoch(SystemTime time)
{
    return std::chrono::duration<double>(time.time_since_epoch()).count();
}


date::year_month_day Time::normalizeDate(date::year_month_day date)
{
    // See https://en.cppreference.com/w/cpp/chrono/year_month_day/operator_days
    date += date::months{0};     // normalizes year and month
    return date::sys_days{date}; // normalizes day
}


date::year_month_day Time::utcDateFromSystemTime(const SystemTime& time)
{
    return std::chrono::time_point_cast<date::days>(time);
}


SystemTime Time::systemTimeFromUtcTimeval(const timeval& time)
{
    auto timePoint = std::chrono::system_clock::from_time_t(time.tv_sec);
    return timePoint + std::chrono::microseconds{time.tv_usec};
}


SystemTime Time::systemTimeFromUtcTm(struct tm& tmInUtc)
{
    // The underlying timepoint is given for UTC, hence the use of gmtime when converting to time_t.
    // As there is no inverse of gmtime we have to settle for mktime. That uses local time whose timezone
    // we now have to compensate.
    static const std::chrono::seconds timezoneOffsetSeconds(getTimezoneOffsetInSeconds());
    time_t timeT = mktime(&tmInUtc);
    return std::chrono::system_clock::from_time_t(timeT) + timezoneOffsetSeconds;
}


std::chrono::steady_clock::duration Time::durationFromString(const std::string& durationString)
{
    const std::string trimmedString = String::trim(durationString);
    Assert(! trimmedString.empty()) << "duration string is empty";

    size_t numberEndOffset = 0;
    const size_t numericalValue = std::stoul(trimmedString, &numberEndOffset);
    Assert(numberEndOffset < trimmedString.size()) << "invalid duration string: no unit";

    std::string unit = String::trim(
        {trimmedString.c_str() + numberEndOffset, trimmedString.size() - numberEndOffset});

    if (unit == "h")
        return std::chrono::hours(numericalValue);
    if (unit == "m" || unit == "min")
        return std::chrono::minutes(numericalValue);
    if (unit == "s")
        return std::chrono::seconds(numericalValue);
    if (unit == "ms")
        return std::chrono::milliseconds(numericalValue);

    Failure() << "invalid duration string unit: \"" << unit
              << "\", expected: h (hours), m (minutes), s (seconds), or ms (milliseconds)";
}


std::string Time::durationToString(std::chrono::nanoseconds duration)
{
    // Formats 1234 as "1.234" and 12 as "0.012".
    const auto dividedBy1000 = [](auto count) {
        std::string result = std::to_string(count);
        if (result.length() < 4)
            result.insert(0, 4 - result.length(), '0');
        result.insert(result.size() - 3, 1, '.');
        return result;
    };

    const std::string sign = duration.count() < 0 ? "-" : "";

    const auto nanoseconds = std::abs(duration.count());
    if (nanoseconds < 1'000)
        return sign + std::to_string(nanoseconds) + "ns";

    const auto microseconds = (nanoseconds + 500) / 1000;
    if (microseconds < 1'000'000)
        return sign + dividedBy1000(microseconds) + "ms";

    const auto milliseconds = (microseconds + 500) / 1000;
    if (milliseconds < 60'000)
        return sign + dividedBy1000(milliseconds) + "s";

    const auto seconds = (milliseconds + 500) / 1000;
    return sign + std::to_string(seconds / 60) + "m" + std::to_string(seconds % 60) + "s";
}


std::string Time::toGermanDateString(const date::year_month_day& date)
{
    return date::format(std::locale::classic(), "%d.%m.%Y", date);
}


template<>
SystemTime JsonReader::fromJson<SystemTime>(const JsonReader& reader)
{
    return Time::fromRfc3339(reader.stringValue());
}


template<>
date::year_month_day JsonReader::fromJson<date::year_month_day>(const JsonReader& reader)
{
    return Time::fromIso8601Date(reader.stringValue());
}


void writeJson(JsonWriter& writer, date::year_month_day value)
{
    writer.makeStringValue(Time::toIso8601Date(value));
}


void writeJson(JsonWriter& writer, SystemTime value)
{
    writer.makeStringValue(Time::toRfc3339(value));
}

} // namespace epa
