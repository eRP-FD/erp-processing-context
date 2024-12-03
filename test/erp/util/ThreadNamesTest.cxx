/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/ThreadNames.hxx"

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
