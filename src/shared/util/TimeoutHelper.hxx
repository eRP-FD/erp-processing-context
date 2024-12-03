#ifndef ERP_PROCESSIONG_CONTEXT_TIMEOUTHELPER_HXX_INCLUDED
#define ERP_PROCESSIONG_CONTEXT_TIMEOUTHELPER_HXX_INCLUDED

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>

class TimeoutHelper
{
public:
    using Strand = boost::asio::strand<boost::asio::any_io_executor>;

    TimeoutHelper(Strand& strand, std::chrono::steady_clock::duration timeout);

    template<typename T>
    auto operator()(T&& timedHandler)
    {
        return bind_cancellation_slot(slot(), std::forward<T>(timedHandler));
    }

    void done();
    bool timedOut() const;
    explicit operator bool();

    boost::asio::cancellation_slot slot();


private:
    void handler(boost::system::error_code ec);

    boost::asio::steady_timer mTimer;
    boost::asio::cancellation_signal mSignal;
    bool mTimedOut = false;
};


#endif// ERP_PROCESSIONG_CONTEXT_TIMEOUTHELPER_HXX_INCLUDED
