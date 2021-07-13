#include "erp/util/TerminationHandler.hxx"

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
    handler.notifyTermination(false);
    handler.waitForTerminated();

    ASSERT_TRUE(callbackCalled);
    ASSERT_EQ(handler.getState(), TerminationHandler::State::Terminated);
}


TEST_F(TerminationHandlerTest, hasError)
{
    LocalTerminationHandler handler;

    ASSERT_FALSE(handler.hasError());

    handler.notifyTermination(true);
    handler.waitForTerminated();

    ASSERT_TRUE(handler.hasError());
}
