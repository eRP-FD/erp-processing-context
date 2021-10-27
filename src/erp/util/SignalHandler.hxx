/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef E_LIBRARY_UTIL_SIGNALHANDLER_HXX
#define E_LIBRARY_UTIL_SIGNALHANDLER_HXX

#include <boost/asio/io_context.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/signal_set.hpp>
#include <memory>
#include <thread>
#include <vector>


/**
 * A central signal handler whose only action on receiving a signal is to notify the TerminationHandler.
 */
class SignalHandler
{
public:
    void registerSignalHandlers(const std::vector<int>& signals);

    explicit SignalHandler (boost::asio::io_context& ioContext);

    /// @brief trigger a process termination from anywhere
    static void manualTermination();

    int mSignal = 0;
private:
    boost::asio::io_context& mIoContext;
    boost::asio::signal_set mSignalSet;
};


#endif
