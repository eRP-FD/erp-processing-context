/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TIMEPERIOD_HXX
#define ERP_PROCESSING_CONTEXT_TIMEPERIOD_HXX

#include "shared/model/Timestamp.hxx"

#include <chrono>


namespace model {

/**
 * Intended use is together with search operations where a partial date is given, i.e. one without month, day, or time.
 *
 * Regarding the use of begin() and end(). In contrast to XML's specification of types that can represent
 * time spans like date, gYearMonth, gYear, where the end value is one minute less than the next exclusive value, end()
 * returns start() plus exactly one day, one month or one year. This is because the FHIR types allow seconds and sub
 * second precision. This would motivate the last inclusive value to be a millisecond before the first exclusive value.

 * In order to avoid this source of confusion, end() returns the first exclusive value of a time period. The caller can
 * easily subtract a minute, a second or a millisecond, depending on the use case.
 *
 * The name of the class and its members is chose to resemble the FHIR Period type but an exact replication of that type
 * is not intended.
 */
class TimePeriod
{
public:
    TimePeriod (model::Timestamp start, model::Timestamp end);

    static TimePeriod fromFhirSearchDate (const std::string& date);
    static TimePeriod fromDatabaseUuid(const std::string& uuid);

    model::Timestamp begin (void) const;
    model::Timestamp end (void) const;

private:
    const model::Timestamp mBegin;
    const model::Timestamp mEnd;
};

}

#endif
