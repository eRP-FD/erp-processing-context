/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_WORKDAY_HXX
#define ERP_PROCESSING_CONTEXT_WORKDAY_HXX

#include "shared/model/Timestamp.hxx"

#include <date/date.h>

// represents a working day, Monday-Saturday, excluding bundeseinheitliche Feiertage
class WorkDay
{
public:
    /// @brief initialize from Timestamp. If baseTime is not a working day, initialize from (baseTime+1day)
    explicit WorkDay(model::Timestamp baseTime);
    explicit WorkDay(const date::year_month_day& day);

    /// @brief add days working days.
    WorkDay operator+(unsigned workingDaysToAdd);

    [[nodiscard]] model::Timestamp toTimestamp() const;
    [[nodiscard]] date::year_month_day getYearMonthDay() const;

    /// @brief returns true, if day is not a sunday and not a public holiday.
    static bool isWorkDay(const date::year_month_day& day);

private:
    date::year_month_day mDay;
};


#endif//ERP_PROCESSING_CONTEXT_WORKDAY_HXX
