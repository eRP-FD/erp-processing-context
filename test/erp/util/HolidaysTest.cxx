#include "erp/util/Holidays.hxx"

#include <date/date.h>
#include <gtest/gtest.h>

#include "erp/util/Environment.hxx"

class HolidaysTest : public testing::Test
{
public:
    const Holidays& holidays()
    {
        if (! mHolidays)
            mHolidays.reset(new Holidays);
        return *mHolidays;
    }

protected:
    void TearDown() override
    {
        Test::TearDown();
        Environment::unset("ERP_SERVICE_TASK_ACTIVATE_HOLIDAYS");
    }

private:
    std::unique_ptr<Holidays> mHolidays;
};

using namespace date::literals;

TEST_F(HolidaysTest, FixedHoliday)// NOLINT
{
    Environment::set("ERP_SERVICE_TASK_ACTIVATE_HOLIDAYS", "01-01;01-02");
    ASSERT_TRUE(holidays().isHoliday(2021_y / 01 / 01));
    ASSERT_TRUE(holidays().isHoliday(2021_y / 01 / 02));
    ASSERT_FALSE(holidays().isHoliday(2021_y / 01 / 03));
    ASSERT_FALSE(holidays().isHoliday(2021_y / 12 / 25));
}


TEST_F(HolidaysTest, FloatingHoliday)// NOLINT
{
    Environment::set("ERP_SERVICE_TASK_ACTIVATE_HOLIDAYS", "Karfreitag;Ostermontag;Himmelfahrt;Pfingstmontag;-1;2");
    ASSERT_FALSE(holidays().isHoliday(2021_y / 04 / 01));
    ASSERT_TRUE(holidays().isHoliday(2021_y / 04 / 02));// Karfreitag
    ASSERT_TRUE(holidays().isHoliday(2021_y / 04 / 03));// -1
    ASSERT_TRUE(holidays().isHoliday(2021_y / 04 / 04));// easter
    ASSERT_TRUE(holidays().isHoliday(2021_y / 04 / 05));// Ostermontag
    ASSERT_TRUE(holidays().isHoliday(2021_y / 04 / 06));// +2
    ASSERT_TRUE(holidays().isHoliday(2021_y / 05 / 13));// Himmelfahrt
    ASSERT_TRUE(holidays().isHoliday(2021_y / 05 / 24));// Pfingstmontag
}

TEST_F(HolidaysTest, OpsConfig)// NOLINT
{
    Environment::set("ERP_SERVICE_TASK_ACTIVATE_HOLIDAYS",
                     "01-01;Karfreitag;Ostermontag;05-01;Himmelfahrt;Pfingstmontag;10-03;12-25;12-26");
    ASSERT_TRUE(holidays().isHoliday(2021_y / 01 / 01));
    ASSERT_TRUE(holidays().isHoliday(2021_y / 04 / 02));
    ASSERT_TRUE(holidays().isHoliday(2021_y / 04 / 04));
    ASSERT_TRUE(holidays().isHoliday(2021_y / 04 / 05));
    ASSERT_TRUE(holidays().isHoliday(2021_y / 5 / 1));
    ASSERT_TRUE(holidays().isHoliday(2021_y / 05 / 13));
    ASSERT_TRUE(holidays().isHoliday(2021_y / 05 / 24));
    ASSERT_TRUE(holidays().isHoliday(2021_y / 10 / 3));
    ASSERT_TRUE(holidays().isHoliday(2021_y / 12 / 25));
    ASSERT_TRUE(holidays().isHoliday(2021_y / 12 / 26));
}


TEST_F(HolidaysTest, NoHoliday)// NOLINT
{
    Environment::set("ERP_SERVICE_TASK_ACTIVATE_HOLIDAYS", "");
    ASSERT_FALSE(holidays().isHoliday(2021_y / 01 / 01));
    ASSERT_FALSE(holidays().isHoliday(2021_y / 04 / 02));
    ASSERT_TRUE(holidays().isHoliday(2021_y / 04 / 04));//easter sunday
    ASSERT_FALSE(holidays().isHoliday(2021_y / 04 / 05));
    ASSERT_FALSE(holidays().isHoliday(2021_y / 5 / 1));
    ASSERT_FALSE(holidays().isHoliday(2021_y / 05 / 13));
    ASSERT_FALSE(holidays().isHoliday(2021_y / 05 / 24));
    ASSERT_FALSE(holidays().isHoliday(2021_y / 10 / 3));
    ASSERT_FALSE(holidays().isHoliday(2021_y / 12 / 25));
    ASSERT_FALSE(holidays().isHoliday(2021_y / 12 / 26));
}
