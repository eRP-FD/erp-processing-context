/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <gtest/gtest.h>

#include "erp/util/WorkDay.hxx"

class WorkDayTest : public testing::Test
{
public:
};

using namespace date::literals;
using namespace date;

TEST_F(WorkDayTest, respectSundayWhenAddingDays)
{
    auto baseDay = 2021_y/April/23; // a Friday
    WorkDay wd(baseDay);
    ASSERT_EQ(wd.getYearMonthDay(), baseDay);
    wd = wd + 2;
    auto expected = 2021_y/April/26; // a Monday
    ASSERT_EQ(wd.getYearMonthDay(), expected);
}


TEST_F(WorkDayTest, respect2SundaysWhenAddingDays)
{
    auto baseDay = 2021_y/July/2; // a Friday
    WorkDay wd(baseDay);
    ASSERT_EQ(wd.getYearMonthDay(), baseDay);
    wd = wd + 8;
    auto expected = 2021_y/July/12; // a Monday
    ASSERT_EQ(wd.getYearMonthDay(), expected);
}


TEST_F(WorkDayTest, respectHolidaysWhenAddingDays)
{
    auto baseDay = 2021_y/April/1; // thursday before easter
    WorkDay wd(baseDay);
    ASSERT_EQ(wd.getYearMonthDay(), baseDay);
    wd = wd + 2;
    auto expected = 2021_y/April/6;
    ASSERT_EQ(wd.getYearMonthDay(), expected);
}


TEST_F(WorkDayTest, constructFromWorkday)
{
    auto baseDay = 2021_y/April/23; // a Friday
    WorkDay wd(baseDay);
    ASSERT_EQ(wd.getYearMonthDay(), baseDay);
}


TEST_F(WorkDayTest, constructFromSunday)
{
    auto baseDay = 2021_y/April/25; // a Sunday
    WorkDay wd(baseDay);
    auto expected = 2021_y/April/26; // a Monday
    ASSERT_EQ(wd.getYearMonthDay(), expected);
}

TEST_F(WorkDayTest, constructFromHoliday)
{
    auto baseDay = 2021_y/April/4; // easter
    WorkDay wd(baseDay);
    auto expected = 2021_y/April/6;
    ASSERT_EQ(wd.getYearMonthDay(), expected);
}
