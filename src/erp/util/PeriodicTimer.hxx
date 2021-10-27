/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_PERIODICTIMER_HXX
#define ERP_PROCESSING_CONTEXT_PERIODICTIMER_HXX

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>

class PeriodicTimer
{
public:
    explicit PeriodicTimer(std::chrono::steady_clock::duration interval);
    void start(boost::asio::io_context& context, std::chrono::steady_clock::duration initialInterval);

    virtual ~PeriodicTimer();


    PeriodicTimer(const PeriodicTimer&) = delete;
    PeriodicTimer(PeriodicTimer&&) = delete;
    PeriodicTimer& operator =(const PeriodicTimer&) = delete;
    PeriodicTimer& operator =(PeriodicTimer&&) = delete;
protected:
    virtual void timerHandler() = 0;

private:
    void timerHandlerInternal(const boost::system::error_code& errorCode);

    std::unique_ptr<boost::asio::steady_timer> mTimer;
    std::chrono::steady_clock::duration mInterval;
};

#endif// ERP_PROCESSING_CONTEXT_PERIODICTIMER_HXX
