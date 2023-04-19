/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/Condition.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>
#include <atomic>
#include <thread>


class ConditionTest : public testing::Test
{
};



TEST_F(ConditionTest, waitForChange)
{
    // Use int instead of enum to verify that that works as well.
    Condition<int> condition (5);
    std::atomic_int state (0);

    std::thread test ([&]
    {
        condition.waitForChange();

        state = 1;
    });

    // Give the test thread time to step over the first `waitForChange` and then verify that it didn't.
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    EXPECT_EQ(state, 0);

    // Change `condition` and verify that the test thread moved forward.
    condition = 1;

    testutils::waitFor([&state] {
        return state == 1;
    });

    // Clean up.
    test.join();
}


TEST_F(ConditionTest, waitForValue)
{
    enum class Values
    {
        A,
        B,
        C
    };
    Condition<Values> condition (Values::A);
    std::atomic_int state (0);

    std::thread test ([&]
    {
        condition.waitForValue(Values::B, std::chrono::seconds(1)); // The timeout is not expected to be triggered.

        state = 1;
    });

    // Give the test thread time to step over the first `waitForValue` and then verify that it didn't.
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    EXPECT_EQ(state, 0);

    // Change `condition` and verify that that did *not* trigger a release of `waitForValue`.
    condition = Values::C;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_EQ(state, 0);

    // Change `condition` to the expected value and verify that that *did* trigger the release of `waitForValue`.
    condition = Values::B;
    testutils::waitFor([&state] {
        return state == 1;
    });

    // Clean up.
    test.join();
}


TEST_F(ConditionTest, waitForValue_timeout)
{
    Condition<double> condition (1.0);

    // Wait for something that does not happen. Then verify that the timeout was triggered.
    condition.waitForValue(2.0, std::chrono::milliseconds(5));

    SUCCEED();
}
