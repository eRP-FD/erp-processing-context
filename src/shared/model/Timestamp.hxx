/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIR_TOOLS_TIMESTAMP_HXX
#define FHIR_TOOLS_TIMESTAMP_HXX

#include <chrono>
#include <string>
#include <date/date.h>

namespace model
{

/**
 *  Representation of a timestamp, i.e. a date and a time, together with methods for parsing and writing
 *  formats that are understood by PostgresDB and FHIR.
 *
 *  Regarding FHIR data types please refer to https://www.hl7.org/fhir/datatypes.html. This class implements an
 *  intersection of what is described on the FHIR page in the "domain value" column and the specification of the
 *  references XML types xs:dateTime, xs:date, xs:gYearMonth, xs:gYear. This leads to this definition of date time values:
 *  - four different formats: YYYY, YYYY-MM, YYYY-MM-DD, YYYY-MM-DDThh:mm:ss+zz:zz
 *    only YYYY-MM-DDThh:mm:ss+zz:zz is currently supported
 *  - no negative years
 *  - where a time is part of the format
 *    - milliseconds are supported
 *    - time zone is parsed in the format Z, +/-hh:ss, or can be missing
 *    - time zone is written as a fixed +00:00, i.e. UTC, this can be changed later
 *  - leap seconds are not supported
 *  - date and time are separated by a 'T' (lower case 't' i snot supported)
 */
class Timestamp
{
public:
    using timepoint_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;
    using duration_t = timepoint_t::duration;

    static constexpr const char* GermanTimezone = "Europe/Berlin";
    static constexpr const char* UTCTimezone = "UTC";
    // 1893-04-01 Europe/Berlin time was introduced
    // Therefore `1893-04-01T00:00:00 CET` is invalid and we accept only times starting with `1893-04-02 00:00:00 CET`
    // in 1893 there was no daylight saving so the offset is -1 hour
    static constexpr timepoint_t minTimeStamp{date::sys_days{date::year{1893} / date::month{4} / date::day{2}} -
                                              std::chrono::hours{1}};

    static Timestamp now ();

    enum class Type
    {
        DateTimeWithFractionalSeconds,
        DateTimeWithoutSeconds,
        DateTime,
        Date,
        YearMonth,
        Year
    };

    static Type detectType (const std::string& dateAndTime);

    /**
     * The FHIR dateTime data type is based on XML types xs:dateTime, xs:date, xs:gYearMonth, xs:gYear with
     * additional restrictions. See https://www.hl7.org/fhir/datatypes.html#dateTime. Also see the class comment for details.
     *
     */
    static Timestamp fromFhirDateTime(const std::string& dateAndTime,
                                      const std::string& fallbackTimezone = GermanTimezone);

    // For search the rules are a little bit different (time zone optional)
    static Timestamp fromFhirSearchDateTime(const std::string& dateAndTime,
                                            const std::string& fallbackTimezone = GermanTimezone);

    /**
     * Read a date time value in the XML xs:dateTime format.
     * Format is YYYY-MM-DDTHH:MM:SS(.SSS)?TZ
     * For xs:dateTime see https://www.w3.org/TR/xmlschema-2/#dateTime
     * Time zone is mandatory per "If hours and minutes are specified, a time zone SHALL be populated."
     * in https://www.hl7.org/fhir/datatypes.html#dateTimeAs and because hours and minutes are mandatory for xs:dataTime.
     * Negative year values are rejected according to the regex also in https://www.hl7.org/fhir/datatypes.html#dateTime.
     */
    static Timestamp fromXsDateTime(const std::string& dateAndTime);

    /**
     * Read a date value in the XML xs:date format.
     * Format is YYYY-MM-DD
     * For xs:date see https://www.w3.org/TR/xmlschema-2/#date.
     * The regex in https://www.hl7.org/fhir/datatypes.html#dateTime imposes some limitations:
     *   - timezone is not supported
     *   - negative year values are not supported
     */
    static Timestamp fromXsDate(const std::string& date, const std::string& timezone);

    // Read a date value in the XML xs:date format, but interpret it as German TZ
    // behaviour:
    //    auto ts = Timestamp::fromGermanDate("2022-07-12");
    //    EXPECT_EQ(ts.toXsDateTime(), "2022-07-11T22:00:00.000+00:00");
    //    EXPECT_EQ(ts.toXsDate(), "2022-07-11");
    //    EXPECT_EQ(ts.toGermanDate(), "2022-07-12");
    static Timestamp fromGermanDate(const std::string& date);

    /**
     * Read a date value in the XML xs:gYearMonth format.
     * Format is YYYY-MM
     * For xs:gYearMonth see https://www.w3.org/TR/xmlschema-2/#gYearMonth
     * The regex in https://www.hl7.org/fhir/datatypes.html#dateTime imposes some limitations:
     *   - timezone is not supported
     *   - negative year values are not supported
     */
    static Timestamp fromXsGYearMonth(const std::string& dateAndTime, const std::string& timezone);

    /**
     * Read a date value in the XML xs:date format.
     * Format is YYYY
     * For xs:gYear see https://www.w3.org/TR/xmlschema-2/#gYear
     * The regex in https://www.hl7.org/fhir/datatypes.html#dateTime imposes some limitations:
     *   - timezone is not supported
     *   - negative year values are not supported
     */
    static Timestamp fromXsGYear(const std::string& dateAndTime, const std::string& timezone);

    /**
     * Read date time value from format YYYYMMDDHHMMSS and assume
     * UTC time zone.
     */
    static Timestamp fromDtmDateTime(const std::string& dateAndTime);

    static Timestamp fromXsTime(const std::string& time);

    /**
     * Convert the given timestamp (sorted)uuid from the database
     * value to a timestamp. Only the first 16 bytes (excluding dashes)
     * are considered for timestamp extraction.
     */
    static Timestamp fromDatabaseSUuid(const std::string& suuid);

    static Timestamp fromTmInUtc(tm tmInUtc);

    template<typename Duration>
    explicit Timestamp (std::chrono::time_point<std::chrono::system_clock, Duration> systemClockTimePoint)
        : Timestamp(std::chrono::time_point_cast<Timestamp::timepoint_t::duration>(systemClockTimePoint))
    {
    }

    explicit Timestamp (Timestamp::timepoint_t dateAndTime);

    explicit Timestamp (int64_t secondsSinceEpoch);
    /**
     * Seconds since 1.1.1970 as fractional seconds, as returned by Postgres' EXTRACT(EPOCH <value>)
     */
    explicit Timestamp (double secondsSinceEpoch);

    explicit Timestamp(const std::string& timezone, const date::local_days& days);

    /**
     * Convert to an an XML xs:dateTime string.
     * - Time zone is added in the form "+00:00" for UTC. This can later be made configurable with a time zone parameter
     *   or auto-detect the current time zone information.
     */
    std::string toXsDateTime () const;

    std::string toXsDateTimeWithoutFractionalSeconds(const std::string& timezone = UTCTimezone) const;

    /**
     * Convert to an XML xs:date format (YYYY-MM-DD).
     */
    std::string toXsDate(const std::string& timezone) const;

    /// see fromGermanDate
    std::string toGermanDate() const;

    /// dd.mm.YYYY
    std::string toGermanDateFormat() const;

    /**
     * Convert to xs:gYearMonth format (YYYY-MM).
     */
    std::string toXsGYearMonth(const std::string& timezone) const;

    /**
     * Convert to xs:gYear format (YYYY).
     */
    std::string toXsGYear(const std::string& timezone) const;

    /**
     * Convert to xs:time format (hh:mm:ss[(+|-)hh:mm]).
     */
    std::string toXsTime() const;

    /**
     * Convert timestamp to a (sortable) uuid as used in the database schema.
     */
    std::string toDatabaseSUuid() const;

    /**
     * Return a std::chrono::system_clock time point so that
     * - two Timestamp objects can be compared to each other and
     * - to allow modification of a Timestamp object by excursion to std::chrono and its manipulator functions.
     */
    timepoint_t toChronoTimePoint () const;

    date::local_days localDay(const std::string& timezone = GermanTimezone) const;
    date::year_month_day utcDay() const;

    /**
     * Convert to timestamp (time_t) format.
     */
    std::time_t toTimeT () const;
    //NOLINTNEXTLINE(hicpp-use-nullptr,modernize-use-nullptr)
    std::strong_ordering operator<=>(const Timestamp& other) const = default;
    Timestamp operator+ (const duration_t& duration) const;
    Timestamp operator- (const duration_t& duration) const;
    duration_t operator- (const Timestamp& timestamp) const;

private:
    static timepoint_t toTimePoint(const std::string& timezone, const date::local_days& days);

    timepoint_t mDateAndTime;
};

// Provide a << stream operator mainly for GTest
std::ostream& operator<< (std::ostream& os, const Timestamp& t);


}

#endif
