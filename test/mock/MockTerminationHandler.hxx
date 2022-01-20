/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

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
    ~MockTerminationHandler() override;

    void terminate() override;
    void gracefulShutdown(boost::asio::io_context& ioContext, int delaySeconds) override;
    void waitForTerminated (void);
};

#endif
