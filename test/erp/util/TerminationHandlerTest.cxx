/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/deprecated/TerminationHandler.hxx"

#include <gtest/gtest.h>
#include <atomic>


#include <optional>

class TerminationHandlerTest : public testing::Test
{
};


class LocalTerminationHandler : public TerminationHandler
{
public:
    LocalTerminationHandler (void) = default;
};


TEST_F(TerminationHandlerTest, initialState)
{
    LocalTerminationHandler handler;
    ASSERT_EQ(handler.getState(), TerminationHandler::State::Running);
}


TEST_F(TerminationHandlerTest, callback)
{
    LocalTerminationHandler handler;
    std::atomic_bool callbackCalled = false;

    handler.registerCallback([&](auto)
    {
        ASSERT_EQ(handler.getState(), TerminationHandler::State::Terminating);
        callbackCalled = true;
    });

    // Initiate termination and wait for its completion.
    handler.notifyTerminationCallbacks(false);

    ASSERT_TRUE(callbackCalled);
    ASSERT_EQ(handler.getState(), TerminationHandler::State::Terminated);
}


TEST_F(TerminationHandlerTest, hasError)
{
    LocalTerminationHandler handler;

    ASSERT_FALSE(handler.hasError());

    handler.notifyTerminationCallbacks(true);

    ASSERT_TRUE(handler.hasError());
}
