/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/ThreadNames.hxx"

#include <gtest/gtest.h>


class ThreadNamesTest : public testing::Test
{
};


TEST_F(ThreadNamesTest, currentThreadName)
{
    auto currentThreadName = ThreadNames::instance().getCurrentThreadName();
    ASSERT_EQ(currentThreadName, "test-runner");

    ThreadNames::instance().setCurrentThreadName("current");
    currentThreadName = ThreadNames::instance().getCurrentThreadName();
    ASSERT_EQ(currentThreadName, "current");

    ThreadNames::instance().setCurrentThreadName("test-runner");
}
