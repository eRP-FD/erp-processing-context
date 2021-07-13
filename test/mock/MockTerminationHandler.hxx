#ifndef ERP_PROCESSING_CONTEXT_MOCKTERMINATIONHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_MOCKTERMINATIONHANDLER_HXX

#include "erp/util/TerminationHandler.hxx"


/**
 * Mock the TerminationHandler by effectively disabling it.
 *
 * If you view each test as a simplified version of running the application, with focus on one specific detail, then
 * the TerminationHandler would have to run after each test. It is not designed to do that. It is intended to run
 * when the application stops and not to be restarted.
 */
class MockTerminationHandler : public TerminationHandler
{
public:
    static void setupForTesting (void);
    static void setupForProduction (void);

    virtual std::optional<size_t> registerCallback (std::function<void(bool hasError)>&&) override;
    virtual void waitForTerminated (void) override;
    virtual State getState (void) const override;
    virtual bool hasError (void) const override;
};

#endif
