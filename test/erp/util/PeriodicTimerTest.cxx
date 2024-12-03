/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/PeriodicTimer.hxx"
#include "test/util/TestUtils.hxx"

#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>
#include <condition_variable>
#include <thread>

static constexpr std::chrono::milliseconds interval(200);
static constexpr std::chrono::milliseconds tolerance(80);

TEST(PeriodicTimerTest, countAndInterval)
{
    struct TestTimerHandler final : public FixedIntervalHandler {
        using FixedIntervalHandler::FixedIntervalHandler;
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
    using TestTimer = PeriodicTimer<TestTimerHandler>;
    boost::asio::io_context ioContext;
    TestTimer testTimer(interval);
    testTimer.start(ioContext, interval);
    testTimer.handler()->expectTrigger = std::chrono::steady_clock::now() + interval;
    ioContext.run_for(interval * 5 + tolerance);
    ioContext.stop();
    EXPECT_EQ(testTimer.handler()->count, 5);
}

TEST(PeriodicTimerTest, skip)
{
    struct TestTimerHandler final : public FixedIntervalHandler {
        using FixedIntervalHandler::FixedIntervalHandler;
        void timerHandler() override
        {
            ++count;
            std::this_thread::sleep_for(interval + tolerance);
        }
        size_t count = 0;
        std::chrono::steady_clock::time_point expectTrigger;
    };
    using TestTimer = PeriodicTimer<TestTimerHandler>;
    boost::asio::io_context ioContext;
    TestTimer testTimer(interval);
    testTimer.start(ioContext, interval);
    testTimer.handler()->expectTrigger = std::chrono::steady_clock::now() + interval;
    ioContext.run_for(interval * 8 + tolerance);
    ioContext.stop();
    EXPECT_EQ(testTimer.handler()->count, 4);
}

TEST(PeriodicTimerTest, deleteWaitsForHandler)
{
    using namespace std::chrono_literals;
    std::atomic_bool started = false;
    std::atomic_bool completed = false;
    struct TestTimerHandler final : public OneShotHandler {
        TestTimerHandler(std::atomic_bool& started, std::atomic_bool & completed)
            : mStarted{started}
            , mCompleted{completed}
        {
        }
        void timerHandler() override
        {
            mStarted = true;
            std::this_thread::sleep_for(interval);
            mCompleted = true;
        }
        std::atomic_bool& mStarted;
        std::atomic_bool& mCompleted;
    };
    using TestTimer = PeriodicTimer<TestTimerHandler>;
    auto testTimer = std::make_shared<TestTimer>(started, completed);
    boost::asio::io_context ioContext;
    testTimer->start(ioContext, tolerance);
    std::thread runner{[&] {
        ioContext.run_for(interval + tolerance * 2);
    }};
    testutils::waitFor([&]() -> bool {
        return started;
    });
    testTimer.reset();
    EXPECT_TRUE(completed);
    ioContext.stop();
    runner.join();
}


TEST(PeriodicTimerTest, exception)
{
    using namespace std::chrono_literals;
    std::atomic_bool started = false;
    std::atomic_bool completed = false;
    struct TestTimerHandler final : public OneShotHandler {
        TestTimerHandler(std::atomic_bool& started, std::atomic_bool& completed)
            : mStarted{started}
            , mCompleted{completed}
        {
        }
        void timerHandler() override
        {
            mStarted = true;
            throw std::runtime_error("test");
            mCompleted = true;
        }
        std::atomic_bool& mStarted;
        std::atomic_bool& mCompleted;
    };

    using TestTimer = PeriodicTimer<TestTimerHandler>;
    auto testTimer = std::make_shared<TestTimer>(started, completed);
    boost::asio::io_context ioContext;

    testTimer->start(ioContext, tolerance);
    std::thread runner{[&] {
        ASSERT_THROW(ioContext.run_for(2 * interval), std::runtime_error);
    }};
    testutils::waitFor([&]() -> bool {
        return started;
    });
    testTimer.reset();
    EXPECT_FALSE(completed);
    ioContext.stop();
    runner.join();
}