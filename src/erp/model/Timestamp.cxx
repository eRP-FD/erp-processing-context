/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Timestamp.hxx"
#include "erp/util/Expect.hxx"

#include <date/date.h>
#include <iomanip>
#include <iostream>
#include <mutex>


namespace model
{

namespace {
    /**
     * Regarding the choice of `const char*` over std::string_view or std::string.
     * std::chrono::parse only excepts std::string. Its documentation states that it
     * calls std::chrono::from_stream which in turn only accepts `const char*`. Therefore
     * it seems best to avoid the intermediate std::string which had to be manipulated to make room
     * for the trailing '\0' amd use `const char*` directly with std::chrono::from_stream.
     */
    constexpr const char* xsDateTimeZ = "%Y-%m-%dT%H:%M:%SZ";
    constexpr const char* xsDateTimeTz = "%Y-%m-%dT%H:%M:%S%Ez";
    constexpr const char* xsDate = "%Y-%m-%d";
    constexpr const char* xsGYearMonth = "%Y-%m-%d";
    constexpr const char* xsGYear = "%Y-%m-%d";

    constexpr const char* xsDateTimeWithoutSecondsZ = "%Y-%m-%dT%H:%MZ";
    constexpr const char* xsDateTimeWithoutSecondsTz = "%Y-%m-%dT%H:%M%Ez";

    int64_t timezoneOffset()
    {
        const std::time_t now = time(nullptr);
        struct tm tmLocal {};
        struct tm tmGm {};
#if _POSIX_C_SOURCE >= 1 || _XOPEN_SOURCE || _BSD_SOURCE || _SVID_SOURCE || _POSIX_SOURCE
        localtime_r(&now, &tmLocal);
        gmtime_r(&now, &tmGm);
#elif defined _WIN32
        _localtime64_s(&tmLocal, &now);
        _gmtime64_s(&tmGm, &now);
#else
        tmLocal = *localtime(&now);
        tmGm = *gmtime(&now);
#endif
        return mktime(&tmLocal) - mktime(&tmGm);
    }

    std::mutex gmTimeMtx;
    tm gmTimeWrapper(const time_t* timer)
    {
        std::lock_guard lock(gmTimeMtx);
        return *std::gmtime(timer);
    }

    std::pair<bool, bool> evaluateTimeZone(
        const std::string& dateAndTime,
        const bool timeZoneIsOptional,
        const size_t minSize)
    {
        const bool hasSubSecondPart = dateAndTime.size() > 19 && dateAndTime[19] == '.';
        size_t timeZoneStart = timeZoneIsOptional ? minSize : minSize - 1;
        if (hasSubSecondPart)
        {
            timeZoneStart = dateAndTime.find_first_of("Z+-", timeZoneStart + 1);
            ModelExpect(timeZoneStart > 20, "sub-second part is empty");
        }

        bool hasTimeZone = dateAndTime.size() > timeZoneStart;
        if(!timeZoneIsOptional)
            ModelExpect(hasTimeZone, "timezone is missing");

        bool timeZoneIsZulu = true;
        if(hasTimeZone)
        {
            timeZoneIsZulu = dateAndTime.size() == timeZoneStart + 1;
            if (! timeZoneIsZulu)
            {
                ModelExpect(dateAndTime.size() == timeZoneStart + 6 /* +HH:MM */, "invalid format");
                ModelExpect(dateAndTime[timeZoneStart + 3] == ':', "expecting two digits for timezone hours and minutes");
            }
        }
        return { hasTimeZone, timeZoneIsZulu };
    }

    void validate(const ::std::string& dateAndTime, bool timeZoneIsOptional, bool missingSeconds, ::std::size_t minSize)
    {
        using namespace std::string_literals;

        // Check the first 16/19 characters (YYYY-MM-DDTHH:MM[:SS]) to sort out cases that are allowed by xs:dateTime but not by FHIR.
        // This concerns width of fields and the leading '-'.
        ModelExpect(dateAndTime.size() >= minSize, "date does not match YYYY-MM-DDTHH:MM"s +
                                                       (missingSeconds ? "[:SS.sss]" : ":SS[.sss]") +
                                                       (timeZoneIsOptional ? "[Z|(+|-)hh:mm]" : "(Z|(+|-)hh:mm)"));
        ModelExpect(dateAndTime[0] != '-', "negative years are not supported");
        ModelExpect(dateAndTime[4] == '-', "expecting four digits for year");
        ModelExpect(dateAndTime[5] != '-', "negative months are not permitted");
        ModelExpect(dateAndTime[7] == '-', "expecting two digits for month");
        ModelExpect(dateAndTime[8] != '-', "negative days are not permitted");
        ModelExpect(dateAndTime[10] == 'T', "expecting two digits for day");
        ModelExpect(dateAndTime[11] != '-', "negative hours are not permitted");
        ModelExpect(dateAndTime[13] == ':', "expecting two digits for hour");
        ModelExpect(dateAndTime[14] != '-', "negative minutes are not permitted");
        ModelExpect(std::isdigit(dateAndTime[15]), "expecting two digits for hours");
        if (! missingSeconds)
        {
            ModelExpect(dateAndTime[16] == ':', "expecting two digits for minutes");
            ModelExpect(dateAndTime[17] != '-', "negative seconds are not permitted");
            ModelExpect(std::isdigit(dateAndTime[18]), "expecting two digits for seconds");

            const auto hasMilliseconds = (dateAndTime.size() >= 24) && (dateAndTime[19] == '.');
            if (hasMilliseconds)
            {
                ModelExpect(dateAndTime[20] != '-', "negative milliseconds are not permitted");
            }
        }
    }

    Timestamp fromXsDateTime(
        const std::string& dateAndTime,
        bool timeZoneIsOptional,
        bool missingSeconds)
    {
        using namespace std::string_literals;

        size_t minSize = timeZoneIsOptional ? 19 : 20;
        if(missingSeconds)
            minSize -= 3;

        validate(dateAndTime, timeZoneIsOptional, missingSeconds, minSize);

        const auto [hasTimeZone, timeZoneIsZulu] = evaluateTimeZone(dateAndTime, timeZoneIsOptional, minSize);
        std::chrono::system_clock::time_point result;
        std::istringstream stream (hasTimeZone ? dateAndTime : dateAndTime + 'Z'); // assume UTC if no time zone provided;
        stream.imbue(std::locale::classic());

        if (timeZoneIsZulu)
        {
            date::from_stream(stream, missingSeconds ? xsDateTimeWithoutSecondsZ : xsDateTimeZ, result);
            ModelExpect( ! stream.fail(), "date time has invalid format");
            return Timestamp(result);
        }
        else
        {
            date::from_stream(stream, missingSeconds ? xsDateTimeWithoutSecondsTz : xsDateTimeTz, result);
            ModelExpect( ! stream.fail(), "date time has invalid format");
            return Timestamp(result);
        }
    }


    Timestamp fromFhirDateTime(
        const std::string& dateAndTime,
        bool isSearch)
    {
        switch(Timestamp::detectType(dateAndTime))
        {
            case Timestamp::Type::Year:
                return Timestamp::fromXsGYear(dateAndTime);

            case Timestamp::Type::YearMonth:
                return Timestamp::fromXsGYearMonth(dateAndTime);

            case Timestamp::Type::Date:
                return Timestamp::fromXsDate(dateAndTime);

            case Timestamp::Type::DateTime:
                return fromXsDateTime(dateAndTime, isSearch/*if true => optional time zone*/, false/*=>seconds exist*/);

            case Timestamp::Type::DateTimeWithFractionalSeconds:
                if(isSearch)
                    ErpFail(HttpStatus::BadRequest, "fractional seconds not supported by FHIR date search");
                return fromXsDateTime(dateAndTime, false, false); // time zone and seconds must exist;

            case Timestamp::Type::DateTimeWithoutSeconds:
                if(!isSearch)
                    ErpFail(HttpStatus::BadRequest, "missing seconds not supported by regular FHIR date");
                return fromXsDateTime(dateAndTime, isSearch, true);  // seconds missing

            default:
                throw std::logic_error("unsupported Timestamp::Type enum value");
        }
    }
}


Timestamp Timestamp::now ()
{
    return Timestamp(std::chrono::system_clock::now());
}


Timestamp Timestamp::fromXsDateTime (const std::string& dateAndTime)
{
    return model::fromXsDateTime(dateAndTime, false/*time zone NOT optional*/, false/*seconds must exist*/);
}


Timestamp Timestamp::fromXsDate (const std::string& date)
{
    // Check the first 10 characters (YYYY-MM-DD) to sort out cases that are allowed by xs:date but not by FHIR.
    // This concerns width of fields and the leading '-'.
    ModelExpect(date.size() == 10, "date does not match YYYY-MM-DD");
    ModelExpect(date[0]!='-', "negative years are not supported");
    ModelExpect(date[4]=='-', "expecting four digits for year");
    ModelExpect(date[7]=='-', "expecting two digits for month and day");

    std::istringstream stream (date);
    stream.imbue(std::locale::classic());

    std::chrono::system_clock::time_point result;
    date::from_stream(stream, xsDate, result);
    ModelExpect(!stream.fail(), "invalid date format");
    ModelExpect(stream.tellg() == 10, "did not read the whole date");

    return Timestamp(result);
}


Timestamp Timestamp::fromXsGYearMonth (const std::string& date)
{
    // Check the first 7 characters (YYYY-MM) to sort out cases that are allowed by xs:date but not by FHIR.
    // This concerns width of fields and the leading '-'.
    ModelExpect(date.size() == 7, "date does not match YYYY-MM");
    ModelExpect(date[0]!='-', "negative years are not supported");
    ModelExpect(date[4]=='-', "expecting four digits for year");

    // It appears that there is either a bug in date::from_stream (and in std::get_time) or, more likely, that these
    // functions don't handle a date without day, i.e. "%Y-%m" or just "%Y". Therefore we splice in a synthetic day
    // and parse that.
    std::string fullDate = date;
    fullDate.insert(7, "-01");
    std::istringstream stream (fullDate);
    stream.imbue(std::locale::classic());

    std::chrono::system_clock::time_point result;
    date::from_stream(stream, xsGYearMonth, result);
    ModelExpect(!stream.fail(), "invalid date format");
    ModelExpect(stream.tellg() == 10, "did not read the whole date");

    return Timestamp(result);
}


Timestamp Timestamp::fromXsGYear (const std::string& date)
{
    // Check the first 4 characters (YYYY) to sort out cases that are allowed by xs:date but not by FHIR.
    // This concerns width of fields and the leading '-'.
    ModelExpect(date.size() == 4, "date does not match YYYY");
    ModelExpect(date[0]!='-', "negative years are not supported");

    // It appears that there is either a bug in date::from_stream (and in std::get_time) or, more likely, that these
    // functions don't handle a date without day, i.e. "%Y-%m" or just "%Y". Therefore we splice in a synthetic day
    // and parse that.
    std::string fullDate = date;
    fullDate.insert(4, "-01-01");
    std::istringstream stream (fullDate);
    stream.imbue(std::locale::classic());

    std::chrono::system_clock::time_point result;
    date::from_stream(stream, xsGYear, result);
    ModelExpect(!stream.fail(), "invalid date format");
    ModelExpect(stream.tellg() == 10, "did not read the whole date");

    return Timestamp(result);
}

Timestamp Timestamp::fromTmInUtc(tm tmInUtc)
{
    // The underlying timepoint is given for UTC, hence the use of gmtime when converting to time_t.
    // As there is no inverse of gmtime we have to settle for mktime. That uses local time whose timezone
    // we now have to compensate.
    time_t timeT = mktime(&tmInUtc);
    const auto timepoint = std::chrono::system_clock::from_time_t(timeT);
    return Timestamp(timepoint + std::chrono::seconds(timezoneOffset()));
}


Timestamp::Type Timestamp::detectType (const std::string& dateAndTime)
{
    switch(dateAndTime.size())
    {
        case 4:
            return Type::Year;

        case 7:
            return Type::YearMonth;

        case 10:
            return Type::Date;

        default:
            if (dateAndTime.size() > 19 && dateAndTime[19] == '.')
                return Type::DateTimeWithFractionalSeconds;
            else if(dateAndTime.size() > 16 && dateAndTime[16] == ':')
                return Type::DateTime;
            else
                return Type::DateTimeWithoutSeconds;
    }
}


Timestamp Timestamp::fromFhirSearchDateTime (const std::string& dateAndTime)
{
    return model::fromFhirDateTime(dateAndTime, true/*isSearch*/);
}


Timestamp Timestamp::fromFhirDateTime (const std::string& dateAndTime)
{
    return model::fromFhirDateTime(dateAndTime, false/*isSearch*/);
}


Timestamp::Timestamp (std::chrono::system_clock::time_point dateAndTime)
    : mDateAndTime(std::chrono::time_point_cast<std::chrono::milliseconds>(dateAndTime))
{
}

Timestamp::Timestamp (const int64_t secondsSinceEpoch)
    : Timestamp(std::chrono::system_clock::time_point(std::chrono::seconds{ secondsSinceEpoch }))
{
}

Timestamp::Timestamp (const double secondsSinceEpoch)
    : Timestamp(std::chrono::system_clock::time_point(std::chrono::milliseconds{static_cast<int64_t>(secondsSinceEpoch*1000)}))
{
}

std::string Timestamp::toXsDateTime() const
{
    const std::time_t t = std::chrono::system_clock::to_time_t(mDateAndTime);
    std::ostringstream s;
    // Use gmtime to avoid time zone conversion.
    const auto tm = gmTimeWrapper(&t);
    s << std::put_time(&tm, "%FT%T")
      // Milliseconds are not supported by put_time. We have to add them manually.
      << '.' << std::setfill('0') << std::setw(3)
      << ::std::chrono::duration_cast<std::chrono::milliseconds>(
             mDateAndTime - ::std::chrono::floor<::std::chrono::seconds>(mDateAndTime))
             .count()
      // Default UTC time zone. The %z specifier does not emit the colon.
      << "+00:00";
    return s.str();
}


std::string Timestamp::toXsDateTimeWithoutFractionalSeconds() const
{
    const std::time_t t = std::chrono::system_clock::to_time_t(mDateAndTime);
    std::ostringstream s;
    // Use gmtime to avoid time zone conversion.
    const auto tm = gmTimeWrapper(&t);
    s << std::put_time(&tm, "%FT%T")
      // Default UTC time zone. The %z specifier does not emit the colon.
      << "+00:00";
    return s.str();
}


std::string Timestamp::toXsDate() const
{
    std::ostringstream s;
    s << date::format("%Y-%m-%d", mDateAndTime);
    return s.str();
}


std::string Timestamp::toXsGYearMonth() const
{
    std::ostringstream s;
    s << date::format("%Y-%m", mDateAndTime);
    return s.str();
}


std::string Timestamp::toXsGYear() const
{
    std::ostringstream s;
    s << date::format("%Y", mDateAndTime);
    return s.str();
}


std::chrono::system_clock::time_point Timestamp::toChronoTimePoint () const
{
    // Though it is technically allowed, a pre-epoch timestamp should not happen in production
    // and the codebase is not prepared to handle such a case correctly.
    ModelExpect(::std::chrono::duration_cast<::std::chrono::seconds>(mDateAndTime.time_since_epoch()).count() >= 0l,
                "Timestamp is before start of epoch");
    return mDateAndTime;
}


bool Timestamp::operator== (const Timestamp& other) const
{
    return mDateAndTime == other.mDateAndTime;
}


bool Timestamp::operator!= (const Timestamp& other) const
{
    return mDateAndTime != other.mDateAndTime;
}


bool Timestamp::operator< (const Timestamp& other) const
{
    return mDateAndTime < other.mDateAndTime;
}


bool Timestamp::operator<= (const Timestamp& other) const
{
    return mDateAndTime <= other.mDateAndTime;
}


bool Timestamp::operator> (const Timestamp& other) const
{
    return mDateAndTime > other.mDateAndTime;
}


bool Timestamp::operator>= (const Timestamp& other) const
{
    return mDateAndTime >= other.mDateAndTime;
}


std::ostream& operator<< (std::ostream& os, const Timestamp& t)
{
    os << t.toXsDateTime();
    return os;
}

Timestamp Timestamp::operator+ (const duration_t& duration) const
{
    return Timestamp(mDateAndTime + duration);
}


} // end of namespace model
