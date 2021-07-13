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


SignalHandler& SignalHandler::instance (void)
{
    static SignalHandler instance;
    return instance;
}


void SignalHandler::registerSignalHandlers(const std::vector<int>& signals)
{
    for (const int signal : signals)
        mSignalSet.add(signal);

    mSignalSet.async_wait(
        [this](boost::system::error_code const&, const int signal)
        {
            // Stop the `io_context`. This will cause `run()` to return immediately.
            std::cerr << "received signal " << signal << ", stopping io_context" << std::endl;
            mIoContext->stop();

            // According to glibc documentation SIGTERM "is the normal way to politely ask a program to terminate."
            // Therefore it is not treated as error.
            TerminationHandler::instance().notifyTermination(signal != SIGTERM);
        });
}



SignalHandler::SignalHandler (void)
    : mIoContext(std::make_shared<boost::asio::io_context>(BOOST_ASIO_CONCURRENCY_HINT_SAFE)),
      mWork(*mIoContext), // Keep the ioContext "busy", i.e. from assuming that there is nothing to do and exit from its run() method.
      mSignalSet(*mIoContext),
      mSignalThread([this]{run(mIoContext);})
{
    // Prepare to not be joined before the application is exited.
    mSignalThread.detach();
}


SignalHandler::~SignalHandler (void)
{
    // Stop the io_context so that SignalHandler::run() is exited.
    mIoContext->stop();
}

void SignalHandler::run (std::shared_ptr<boost::asio::io_context> ioContext)
{
    ThreadNames::instance().setCurrentThreadName("signal");
    ioContext->run();
    std::cerr << "SignalHandler: leaving thread.";
}
