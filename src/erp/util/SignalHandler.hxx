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

    static SignalHandler& instance (void);

private:
    std::shared_ptr<boost::asio::io_context> mIoContext;
    boost::asio::io_service::work mWork;
    boost::asio::signal_set mSignalSet;
    std::thread mSignalThread;

    SignalHandler (void);
    ~SignalHandler (void);

    static void run(std::shared_ptr<boost::asio::io_context> ioContext);
};


#endif
