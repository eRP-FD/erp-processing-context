/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_TIME_HXX
#define EPA_LIBRARY_UTIL_TIME_HXX

#include "library/json/JsonReader.hxx"

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
// date::* is in std::chrono in C++20; remove this after upgrading.
#include <date/date.h>

namespace epa
{

/**
 * Shorthand for std::chrono::system_clock::time_point, which we assume always has its epoch at
 * 1970-01-01.
 */
using SystemTime = std::chrono::system_clock::time_point;

/**
 * Shorthand for std::chrono::steady_clock::time_point.
 */
using SteadyTime = std::chrono::steady_clock::time_point;


class TimeFormatException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};


/**
 * This is a convenience class that provides conversion functions for frequently used sub-formats
 * of RFC3339 and ISO8601. It uses functions from C and C++ standard libraries where possible.
 */
class Time
{
public:
    static constexpr const char* GermanTimezone = "Europe/Berlin";

    /**
     * Formats a timestamp according to RFC 3339: https://tools.ietf.org/html/rfc3339
     * which is a subset of ISO 8601: https://en.wikipedia.org/wiki/ISO_8601
     *
     * The resulting format is "YYYY-MM-DDThh:mm:ssZ", with either 0, 3, or 6 digits of sub-second
     * precision that we have.
     */
    static std::string toRfc3339(SystemTime time);

    /**
     * Parses a time point from a string formatted as RFC 3339:
     * https://tools.ietf.org/html/rfc3339
     *
     * @throw std::invalid_argument if the input string is malformed
     */
    static SystemTime fromRfc3339(const std::string& rfc3339);

    /**
     * Formats a C++ timestamp in DTM format.
     * DTM is defined in IHE-ITI-TF3; it is "YYYY[MM[DD[hh[mm[ss]]]]]", where the right-most units
     * are optional. But we always format all components, since we cannot know how precise the input
     * is.
     */
    static std::string toDtm(SystemTime time);

    /**
     * Convert a string representation (e.g. 20191224141518) of a time point to a SystemTime.
     * The format of these values is defined as the following regular expression:
       YYYY[MM[DD[hh[mm[ss]]]]]
       Where:
       YYYY is the four digit year i.e., 2006
       MM is the two digit month 01-12, where Jan is 01, Feb is 02, etc.
       DD is the two digit day of the month 01-31
       HH is the two digit hour, 00-23, where 00 is midnight, 01 is 1 am, 12 is noon, 13 is 1 pm, etc.
       mm is the two digit minute, 00-59
       ss is the two digit seconds, 00-59
       The following are legal date time values with increasing precision representing the date and
       time January 2, 2005, 3:04:05am
       2005
       200501
       20050102
       2005010203
       200501020304
       20050102030405

     * @param dtm String with time_point in format YYYYMMDDHHMMSS e.g. "20191224141518"
     * @return Time point that was represented by input data.
     *
     * @throw TimeFormatException if this is not a valid DTM string
     */
    static SystemTime fromDtm(std::string dtm);

    /**
     * Transforms seconds as integer representation for the time past since epoch to the
     * corresponding time_point.
     */
    static SystemTime fromSecondsSinceEpoch(const std::int64_t seconds);

    /**
     * Transforms seconds as double representation for the time past since epoch to the
     * corresponding time_point.
     */
    static SystemTime fromDoubleSecondsSinceEpoch(double seconds);

    /**
     * Inverse function of fromDoubleSecondsSinceEpoch(..).
     */
    static double toDoubleSecondsSinceEpoch(SystemTime time);

    /**
     * Convert the given time to an ISO 8601 basic format string "yyyyMMddThhMMssZ"
     * with date AND time and with second precision.
     * The resulting format is "YYYYMMDDThhmmssZ".
     */
    static std::string toIso8601DateAndTimeBasic(SystemTime time);

    /**
     * Convert the given time to an ISO 8601 basic format string "yyyyMMdd"
     * with date.
     */
    static std::string toIso8601DateBasic(SystemTime time);

    /**
     * Formats a date as YYYY-MM-DD, the short date format given in ISO 8601:
     * https://en.wikipedia.org/wiki/ISO_8601
     */
    static std::string toIso8601Date(date::year_month_day date);

    /**
     * Convert the given time_point to an ISO 8601 string with date AND time and with microsecond
     * precision but without time zone designator. The implicit time zone is UTC.
     * The resulting format is "YYYY-MMDD-Thh:mm:ss.ssssss".
     */
    static std::string toIso8601DateAndTimeUtc(const SystemTime time);

    /**
     * Parses a time point from a string formatted as ISO 8601
     */
    static date::year_month_day fromIso8601Date(const std::string& iso8601Date);

    /**
     * Parses a date of exactly the form YYYY-MM-DD.
     */
    static date::year_month_day fromInternationalDate(const std::string& internationalDate);

    /**
     * Convert the given string in ISO 8601 format with date and time and without time zone
     * designator (implicit time zone is UTC) to a time_point with microsecond precision.
     */
    static SystemTime fromIso8601DateAndTimeUtc(const std::string& iso8601DateAndTime);

    /**
     * convert date to the count of days from epoch begin
     */
    static int toDaysCount(date::year_month_day ymd);

    /**
     * convert the count of days from epoch begin to the date
     */
    static date::year_month_day fromDaysCount(const int countOfDays);

    /**
     * It appears that adding months to a date can lead to invalid dates due to the different number of
     * days in a month.
     * If for example you go from July 31 two months into the future you end up with September 31, which does not
     * exist.
     * This function takes an arbitrary date and moves it to the next valid one.
     */
    static date::year_month_day normalizeDate(date::year_month_day date);

    /**
     * Get the date of a time point, in UTC.
     */
    static date::year_month_day utcDateFromSystemTime(const SystemTime& time);

    /**
     * Convert the UTC time represented by the given timeval to a SystemTime.
     */
    static SystemTime systemTimeFromUtcTimeval(const struct timeval& time);

    /**
     * Convert the UTC time represented by the given tm structure to a SystemTime.
     */
    static SystemTime systemTimeFromUtcTm(struct tm& tmInUtc);

    static std::chrono::steady_clock::duration durationFromString(
        const std::string& durationString);

    /**
     * Converts the given duration to a string that is appropriate for logging.
     *
     * Example: durationToString(std::chrono::milliseconds{123]) -> "123ms"
     *
     * Note: This is not (yet) the complete inverse of durationFromString!
     */
    static std::string durationToString(std::chrono::nanoseconds duration);

    /**
     * Formats the given date in DD.MM.YYYY format.
     */
    static std::string toGermanDateString(const date::year_month_day& date);
};


template<>
SystemTime JsonReader::fromJson<SystemTime>(const JsonReader& reader);

template<>
date::year_month_day JsonReader::fromJson<date::year_month_day>(const JsonReader& reader);

void writeJson(class JsonWriter& writer, date::year_month_day value);
void writeJson(class JsonWriter& writer, SystemTime value);

} // namespace epa

#endif
