/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef FHIR_TOOLS_TIMESTAMP_HXX
#define FHIR_TOOLS_TIMESTAMP_HXX

#include <chrono>
#include <string>

namespace fhirtools
{

/**
 *  Representation of a timestamp, i.e. a date and a time, together with methods for parsing and writing
 *  formats that are understood by PostgresDB and FHIR.
 *
 *  Regarding FHIR data types please refer to https://www.hl7.org/fhir/datatypes.html. This class implements an
 *  intersection of what is described on the FHIR page in the "domain value" column and the the specification of the
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
    using timepoint_t = std::chrono::system_clock::time_point;
    using duration_t = std::chrono::system_clock::duration;

    static constexpr const char* GermanTimezone = "Europe/Berlin";

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
     */
    static Timestamp fromFhirDateTime (const std::string& dateAndTime);

    // from http://hl7.org/fhirpath/#time, without "@T"
    // opposing to xs:time, the literal also supports incomplete timestamps such as 12:00 or 12
    static Timestamp fromFhirPathTimeLiteral(const std::string& time);

    // from http://hl7.org/fhirpath/#datetime, without "@"
    // opposing to xs:dateTime, the literal also supports incomplete timestamps such as 12:00 or 12
    static Timestamp fromFhirPathDateTimeLiteral(const std::string& time);

    // For search the rules are a little bit different (time zone optional)
    static Timestamp fromFhirSearchDateTime (const std::string& dateAndTime);

    /**
     * Read a date time value in the XML xs:dateTime format.
     * Format is YYYY-MM-DDTHH:MM:SS(.SSS)?TZ
     * For xs:dateTime see https://www.w3.org/TR/xmlschema-2/#dateTime
     * Time zone is mandatory per "If hours and minutes are specified, a time zone SHALL be populated."
     * in https://www.hl7.org/fhir/datatypes.html#dateTimeAs and because hours and minutes are mandatory for xs:dataTime.
     * Negative year values are rejected according to the regex also in https://www.hl7.org/fhir/datatypes.html#dateTime.
     */
    static Timestamp fromXsDateTime (const std::string& dateAndTime);

    /**
     * Read a date value in the XML xs:date format.
     * Format is YYYY-MM-DD
     * For xs:date see https://www.w3.org/TR/xmlschema-2/#date.
     * The regex in https://www.hl7.org/fhir/datatypes.html#dateTime imposes some limitations:
     *   - timezone is not supported
     *   - negative year values are not supported
     */
    static Timestamp fromXsDate (const std::string& date);

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
    static Timestamp fromXsGYearMonth (const std::string& dateAndTime);

    /**
     * Read a date value in the XML xs:date format.
     * Format is YYYY
     * For xs:gYear see https://www.w3.org/TR/xmlschema-2/#gYear
     * The regex in https://www.hl7.org/fhir/datatypes.html#dateTime imposes some limitations:
     *   - timezone is not supported
     *   - negative year values are not supported
     */
    static Timestamp fromXsGYear (const std::string& dateAndTime);

    static Timestamp fromXsTime(const std::string& time);

    static Timestamp fromTmInUtc(tm tmInUtc);

    explicit Timestamp (std::chrono::system_clock::time_point dateAndTime);

    explicit Timestamp (int64_t secondsSinceEpoch);
    /**
     * Seconds since 1.1.1970 as fractional seconds, as returned by Postgres' EXTRACT(EPOCH <value>)
     */
    explicit Timestamp (double secondsSinceEpoch);

    /**
     * Convert to an an XML xs:dateTime string.
     * - Time zone is added in the form "+00:00" for UTC. This can later be made configurable with a time zone parameter
     *   or auto-detect the current time zone information.
     */
    std::string toXsDateTime () const;

    std::string toXsDateTimeWithoutFractionalSeconds () const;

    /**
     * Convert to an XML xs:date format (YYYY-MM-DD).
     */
    std::string toXsDate () const;

    /// see fromGermanDate
    std::string toGermanDate() const;

    /**
     * Convert to xs:gYearMonth format (YYYY-MM).
     */
    std::string toXsGYearMonth (void) const;

    /**
     * Convert to xs:gYear format (YYYY).
     */
    std::string toXsGYear (void) const;

    /**
     * Convert to xs:time format (hh:mm:ss[(+|-)hh:mm]).
     */
    std::string toXsTime() const;

    /**
     * Return a std::chrono::system_clock time point so that
     * - two Timestamp objects can be compared to each other and
     * - to allow modification of a Timestamp object by excursion to std::chrono and its manipulator functions.
     */
    timepoint_t toChronoTimePoint () const;

    /**
     * Convert to timestamp (time_t) format.
     */
    std::time_t toTimeT () const;
    //NOLINTNEXTLINE(hicpp-use-nullptr,modernize-use-nullptr)
    std::strong_ordering operator<=>(const Timestamp& other) const = default;
    Timestamp operator+ (const duration_t& duration) const;

private:
    timepoint_t mDateAndTime;
};

// Provide a << stream operator mainly for GTest
std::ostream& operator<< (std::ostream& os, const Timestamp& t);


}

#endif
