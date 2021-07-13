#include "test/mock/MockTerminationHandler.hxx"


void MockTerminationHandler::setupForTesting (void)
{
    setRawInstance(std::make_unique<MockTerminationHandler>());
}


void MockTerminationHandler::setupForProduction (void)
{
    class ProductionTerminationHandler : public TerminationHandler {}; // Expose the constructor
    setRawInstance(std::make_unique<ProductionTerminationHandler>());
}


std::optional<size_t> MockTerminationHandler::registerCallback (std::function<void(bool hasError)>&&)
{
    // Ignore the callback.
    return {};
}


void MockTerminationHandler::waitForTerminated (void)
{
    // Nothing to wait on.
}


TerminationHandler::State MockTerminationHandler::getState (void) const
{
    return State::Running;
}


bool MockTerminationHandler::hasError (void) const
{
    return false;
}

