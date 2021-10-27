/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_HOLIDAYS_HXX
#define ERP_PROCESSING_CONTEXT_HOLIDAYS_HXX

#include <date/date.h>
#include <map>
#include <set>

#include "erp/util/Configuration.hxx"

// named holidays relative to easter sunday
enum class NamedHolidays
{
    Karfreitag,
    Ostermontag,
    Himmelfahrt,
    Pfingstmontag
};

class Holidays
{
public:
    Holidays(const Holidays&) = delete;
    Holidays(Holidays&&) = delete;
    Holidays& operator=(const Holidays&) = delete;
    Holidays& operator=(Holidays&&) = delete;

    static const Holidays& instance();

    bool isHoliday(date::year_month_day yearMonthDay) const;

private:
    Holidays();

    void loadEasterDates(const Configuration& configuration);
    void loadConfiguredHolidays(const Configuration& configuration);

    std::map<date::year, date::year_month_day> mEasterDates;
    std::set<date::month_day> mFixedDates;
    std::set<date::days> mRelativeToEasterDates;

    // we need multiple instances in test.
    friend class HolidaysTest;
};


#endif//ERP_PROCESSING_CONTEXT_HOLIDAYS_HXX
