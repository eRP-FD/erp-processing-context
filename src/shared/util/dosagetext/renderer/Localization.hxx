/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "shared/model/DosageDgMP.hxx"

#include <string_view>

namespace dosagetext
{

enum class GrammaticalNumber
{
    SINGULAR,
    PLURAL
};
bool isSingular(GrammaticalNumber form);

std::string_view germanTimeUnit(model::TimingDpMpTimeUnit timeUnit, GrammaticalNumber form);
std::string_view germanFrequency(model::TimingDpMpTimeUnit timeUnit);
std::string_view germanWeekdayAdverb(model::DaysOfWeek day);
std::string_view germanEventTiming(model::EventTiming eventTiming);
std::string germanDecimal(std::string_view number);
std::string germanTimeOfDay(std::string_view timeOfDay);
std::string_view englishInvalidOrUnsupported();
}// dosagetext
