/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/deprecated/SignalHandler.hxx"
#include "shared/deprecated/TerminationHandler.hxx"

#include "shared/util/TLog.hxx"

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

    if (::signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    {
        TLOG(ERROR) << "SIGPIPE handler not installed.";
    }
}


void SignalHandler::manualTermination()
{
    TLOG(INFO) << "SignalHandler::manualTermination() called, raising internalTerminationSignal "
                  << internalTerminationSignal;
    if (std::raise(internalTerminationSignal) != 0)
    {
        TLOG(ERROR) << "Failed to send signal.";
        exit(EXIT_FAILURE); //NOLINT(concurrency-mt-unsafe)
    }
}

void SignalHandler::gracefulShutdown()
{
    TLOG(INFO) << "SignalHandler::gracefulShutdown() called, raising SIGTERM";
    if (std::raise(SIGTERM) != 0)
    {
        TLOG(ERROR) << "Failed to send signal.";
        exit(EXIT_FAILURE); //NOLINT(concurrency-mt-unsafe)
    }
}


SignalHandler::SignalHandler (boost::asio::io_context& ioContext)
    : mIoContext(ioContext),
      mSignalSet(mIoContext)
{
}
