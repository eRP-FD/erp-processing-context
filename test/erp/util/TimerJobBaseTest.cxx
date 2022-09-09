/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/TimerJobBase.hxx"

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

static constexpr std::chrono::milliseconds interval(200);
static constexpr std::chrono::milliseconds longInterval(interval * 3);
static constexpr std::chrono::milliseconds tolerance(70);

TEST(TimerJobBaseTest, updatedInterval)
{
    struct TestTimer final : public TimerJobBase {
        TestTimer(::std::chrono::steady_clock::duration interval)
            : TimerJobBase{"TimerJobBaseTest", interval}
        {
        }
        void executeJob() override
        {
            using namespace std::chrono;
            ++count;
            auto now = std::chrono::steady_clock::now();
            EXPECT_GE(now, expectTrigger - tolerance) << duration_cast<milliseconds>(now - expectTrigger).count();
            EXPECT_LE(now, expectTrigger + tolerance) << duration_cast<milliseconds>(now - expectTrigger).count();
            expectTrigger += getInterval();
        }
        void setInterval(std::chrono::steady_clock::duration newInterval)
        {
            TimerJobBase::setInterval(newInterval);
        }
        void onFinish() override
        {
        }
        void onStart() override
        {
        }
        size_t count = 0;
        std::chrono::steady_clock::time_point expectTrigger;
    };
    TestTimer testTimer(interval);
    testTimer.start();
    testTimer.expectTrigger = std::chrono::steady_clock::now() + interval;
    std::this_thread::sleep_for(interval * 5 + tolerance);
    EXPECT_EQ(testTimer.count, 5);
    testTimer.setInterval(longInterval);
    std::this_thread::sleep_for(longInterval * 5); // main thread is already behind by `tolerance` no need to add more
    EXPECT_EQ(testTimer.count, 10);
    testTimer.shutdown();
}
