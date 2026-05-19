/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/deprecated/Timer.hxx"
#include "shared/util/TLog.hxx"
#include "test/util/ErpMacros.hxx"

#include <gtest/gtest.h>
#include <array>
#include <chrono>
#include <condition_variable>


/**
 * Note that the tests in this file are sensitive to timing.
 * The used delays should be fairly stable but without extensive use of condition_variables the fact that tests
 * may fail from time to time will not go away.
 * If it turns out that that happens to often, then we may have to think about a better test setup.
 */
class TimerTest : public testing::Test
{
};


/**
 * Usually we would use the Timer::instance() instance but we want to stop the timer explicitly without
 * affecting other, more complex tests that may rely on the global Timer::instance().
 * Therefore we make the constructor accessible, so that a new instance can be created for each test.
 */
class LocalTimer : public Timer
{
public:
    LocalTimer (void) = default;
};


/**
 * Futures and promises are almost able to do what is done with the SharedValue class. But there are
 * some cases where there is a feature missing from std::future.
 *
 * SharedValue is used to set a value from within a timed job and wait for it in the test.
 */
class SharedValue
{
public:
    void append (const std::string& s)
    {
        {
            std::lock_guard lock (mMutex);
            mValue += s;
            ++mUpdateCount;
        }
        mConditionVariable.notify_all();
    }
    std::string get (void)
    {
        std::lock_guard lock (mMutex);
        return mValue;
    }
    size_t updateCount (void)
    {
        std::lock_guard lock (mMutex);
        return mUpdateCount;
    }
    void waitForUpdateCount (const std::chrono::system_clock::duration timeToWait, const size_t expectedCount)
    {
        std::unique_lock lock (mMutex);
        mConditionVariable.wait_for(lock, timeToWait, [&]{return mUpdateCount >= expectedCount;});
    }

private:
    std::mutex mMutex;
    std::condition_variable mConditionVariable;
    std::string mValue;
    size_t mUpdateCount = 0;
};


TEST(TestFrameWork, WithRetriesDoesTry)
{
    size_t tries = 0;
    WITH_RETRIES()
    {
        SOFT_EXPECT_TRUE(tries > 5);
        ++tries;
    }
    EXPECT_GT(tries, 5);
}

TEST(TestFrameWork, DISABLED_WithRetriesCanFail)
{
    WITH_RETRIES(2)
    {
        SOFT_EXPECT_TRUE(false);
    }
}


TEST_F(TimerTest, runAt_now)
{
    WITH_RETRIES()
    {
        LocalTimer timer;
        SharedValue value;

        timer.runAt(std::chrono::system_clock::now(), [&value]{value.append("test");});
        ASSERT_EQ(value.updateCount(), 0);
        value.waitForUpdateCount(std::chrono::milliseconds(100), 1);
        SOFT_EXPECT_TRUE(value.updateCount() == 1);
        SOFT_EXPECT_TRUE(value.get() == "test");
    }
}


TEST_F(TimerTest, runAt_inTheNearFuture)
{
    WITH_RETRIES()
    {
        LocalTimer timer;
        SharedValue value;

        timer.runAt(std::chrono::system_clock::now() + std::chrono::milliseconds(100), [&value]{value.append("test"); });

        ASSERT_EQ(value.updateCount(), 0);
        value.waitForUpdateCount(std::chrono::milliseconds(200), 1);
        SOFT_EXPECT_TRUE(value.updateCount() == 1);
        SOFT_EXPECT_TRUE(value.get() == "test");
    }
}


TEST_F(TimerTest, runIn)
{
    WITH_RETRIES()
    {
        LocalTimer timer;
        SharedValue value;

        timer.runIn(std::chrono::milliseconds(100), [&value]{value.append("test"); });

        ASSERT_EQ(value.updateCount(), 0);
        value.waitForUpdateCount(std::chrono::milliseconds(200), 1);
        SOFT_EXPECT_TRUE(value.updateCount() == 1);
        SOFT_EXPECT_TRUE(value.get() == "test");
    }
}


TEST_F(TimerTest, runIn_shutdownBeforeJobTrigger)
{
    WITH_RETRIES()
    {
        SharedValue value;

        {
            LocalTimer timer;

            timer.runIn(std::chrono::seconds(1), [&value]{value.append("test"); });

            SOFT_EXPECT_TRUE(value.updateCount() == 0);
        }

        // The timer has been shutdown. Verify that the job still has not been run.
        SOFT_EXPECT_TRUE(value.updateCount() == 0);
    }
}


TEST_F(TimerTest, runRepeating)
{
    WITH_RETRIES()
    {
        LocalTimer timer;
        SharedValue value;
        size_t index = 0;

        timer.runRepeating(std::chrono::milliseconds(500), std::chrono::milliseconds(1000), [&]
        {
            value.append(" " + std::to_string(index++));
        });

        ASSERT_EQ(value.updateCount(), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        SOFT_EXPECT_TRUE(value.updateCount() == 3);
        SOFT_EXPECT_TRUE(value.get() == " 0 1 2");
        SOFT_EXPECT_TRUE(index == 3);
    }
}


TEST_F(TimerTest, runRepeating_multipleJobs)
{
    WITH_RETRIES()
    {
        LocalTimer timer;

        constexpr size_t jobCount = 10;

        std::array<SharedValue, jobCount> values;
        std::array<size_t, jobCount> indices{};
        indices.fill(0);

        for (size_t index=0; index<jobCount; ++index)
        {
            timer.runRepeating(std::chrono::milliseconds(500), std::chrono::milliseconds(1000), [&,index]
            {
                values[index].append(" " + std::to_string(indices[index]++));
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        for (size_t index=0; index<jobCount; ++index)
        {
            SOFT_EXPECT_TRUE(values[index].updateCount() == 3);
            SOFT_EXPECT_TRUE(values[index].get() == " 0 1 2");
            SOFT_EXPECT_TRUE(indices[index] == 3);
        }
    }
}


TEST_F(TimerTest, cancel_oneTimeJob)
{
    WITH_RETRIES()
    {
        LocalTimer timer;
        SharedValue value;

        const auto token = timer.runIn(std::chrono::milliseconds(100), [&value]{value.append("test"); });

        ASSERT_EQ(value.updateCount(), 0);

        timer.cancel(token);

        // Give the job time too be triggered but note that that is expected not to happen.
        value.waitForUpdateCount(std::chrono::milliseconds(200), 1);
        SOFT_EXPECT_TRUE(value.updateCount() == 0);
    }
}


TEST_F(TimerTest, cancel_repeatingJob)
{
    WITH_RETRIES()
    {
        LocalTimer timer;
        SharedValue value;

        const auto token = timer.runRepeating(
            std::chrono::milliseconds(50),
            std::chrono::milliseconds(100),
            [&value]{value.append("test"); });

        ASSERT_EQ(value.updateCount(), 0);

        timer.cancel(token);

        // Give the job time too be triggered but note that that is expected not to happen.
        value.waitForUpdateCount(std::chrono::milliseconds(200), 1);
        SOFT_EXPECT_TRUE(value.updateCount() == 0);
    }
}




TEST_F(TimerTest, cancel_repeatingJobAfterFirstTrigger)
{
    WITH_RETRIES()
    {
        LocalTimer timer;
        SharedValue value;

        const auto token = timer.runRepeating(
            std::chrono::milliseconds(150),
            std::chrono::milliseconds(700),
            [&value]{value.append("test"); });

        EXPECT_EQ(value.updateCount(), 0);

        // Wait for the job to be executed once.
        value.waitForUpdateCount(std::chrono::milliseconds(350), 1);
        EXPECT_EQ(value.updateCount(), 1);

        // Now cancel to prevent a second trigger.
        timer.cancel(token);

        // Give the job time too be triggered again but note that that is expected not to happen.
        value.waitForUpdateCount(std::chrono::milliseconds(1000), 2);
        SOFT_EXPECT_TRUE(value.updateCount() == 1);
        SOFT_EXPECT_TRUE(value.get() == "test");
    }
}
