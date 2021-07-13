#include "erp/util/ThreadNames.hxx"

#include <gtest/gtest.h>


class ThreadNamesTest : public testing::Test
{
};


TEST_F(ThreadNamesTest, currentThreadName)
{
    auto currentThreadName = ThreadNames::instance().getCurrentThreadName();
    // ThreadNames::instance()'s life time spans all tests. There is one test that sets the name of the main thread
    // but it is disabled by default. To be on the safe side we allow this and the default name.
    ASSERT_TRUE(currentThreadName=="unknown-thread" || currentThreadName=="test-main");

    ThreadNames::instance().setCurrentThreadName("current");
    currentThreadName = ThreadNames::instance().getCurrentThreadName();
    ASSERT_EQ(currentThreadName, "current");
}