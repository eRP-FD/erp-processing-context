/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
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
