#include "erp/util/PeriodicTimer.hxx"

#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>
#include <thread>

static constexpr std::chrono::milliseconds interval(100);
static constexpr std::chrono::milliseconds tolerance(20);

TEST(PeriodicTimerTest, countAndInterval)
{
    struct TestTimer final : public PeriodicTimer
    {
        using PeriodicTimer::PeriodicTimer;
        void timerHandler() override
        {
            ++count;
            auto now = std::chrono::steady_clock::now();
            EXPECT_GE(now, expectTrigger);
            EXPECT_LE(now, expectTrigger + tolerance);
            expectTrigger += interval;
            // sleep a bit to check if the timer drifts due to execution times in handler:
            std::this_thread::sleep_for(2 * tolerance);
        }
        size_t count = 0;
        std::chrono::steady_clock::time_point expectTrigger;
    };
    boost::asio::io_context ioContext;
    TestTimer testTimer(interval);
    testTimer.start(ioContext);
    testTimer.expectTrigger = std::chrono::steady_clock::now() + interval;
    ioContext.run_for(interval * 5 + tolerance);
    ioContext.stop();
    EXPECT_EQ(testTimer.count, 5);
}

TEST(PeriodicTimerTest, skip)
{
    struct TestTimer final : public PeriodicTimer
    {
        using PeriodicTimer::PeriodicTimer;
        void timerHandler() override
        {
            ++count;
            std::this_thread::sleep_for(interval + tolerance);
        }
        size_t count = 0;
        std::chrono::steady_clock::time_point expectTrigger;
    };
    boost::asio::io_context ioContext;
    TestTimer testTimer(interval);
    testTimer.start(ioContext);
    testTimer.expectTrigger = std::chrono::steady_clock::now() + interval;
    ioContext.run_for(interval * 8 + tolerance);
    ioContext.stop();
    EXPECT_EQ(testTimer.count, 4);
}
