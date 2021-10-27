/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/mock/MockTerminationHandler.hxx"

#include <thread>


void MockTerminationHandler::setupForTesting (void)
{
    setRawInstance(std::make_unique<MockTerminationHandler>());
}


void MockTerminationHandler::setupForProduction (void)
{
    class ProductionTerminationHandler : public TerminationHandler {}; // Expose the constructor
    setRawInstance(std::make_unique<ProductionTerminationHandler>());
}


void MockTerminationHandler::terminate()
{
    // do not actually terminate, only call the termination handlers.
    notifyTerminationCallbacks(true);
}

void MockTerminationHandler::waitForTerminated(void)
{
    while (getState() != TerminationHandler::State::Terminated)
    {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(100ms);
    }
}
