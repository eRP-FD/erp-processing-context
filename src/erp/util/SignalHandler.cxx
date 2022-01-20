/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/SignalHandler.hxx"
#include "erp/util/TerminationHandler.hxx"

#include "erp/util/TerminationHandler.hxx"
#include "TLog.hxx"

#ifndef _WIN32
#include <execinfo.h>
#include <unistd.h>
#endif
#include <csignal>
#include <iostream>

static constexpr int internalTerminationSignal = SIGUSR1;

void SignalHandler::registerSignalHandlers(const std::vector<int>& signals)
{
    for (const int signal : signals)
        mSignalSet.add(signal);
    mSignalSet.add(internalTerminationSignal);

    mSignalSet.async_wait(
        [this](boost::system::error_code const&, const int signal)
        {
            mSignal = signal;
            // Stop the `io_context`. This will cause `run()` to return immediately.
            std::cerr << "received signal " << signal << ", stopping io_context" << std::endl;
            mIoContext.stop();
        });
}


void SignalHandler::manualTermination()
{
    TLOG(WARNING) << "SignalHandler::manualTermination() called, raising internalTerminationSignal "
                  << internalTerminationSignal;
    std::raise(internalTerminationSignal);
}

void SignalHandler::gracefulShutdown()
{
    TLOG(WARNING) << "SignalHandler::gracefulShutdown() called, raising SIGTERM";
    std::raise(SIGTERM);
}


SignalHandler::SignalHandler (boost::asio::io_context& ioContext)
    : mIoContext(ioContext),
      mSignalSet(mIoContext)
{
}