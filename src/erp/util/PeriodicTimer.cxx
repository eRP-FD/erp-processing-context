/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "PeriodicTimer.hxx"

#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"

PeriodicTimer::PeriodicTimer(std::chrono::steady_clock::duration interval)
    : mTimer()
    , mInterval(interval)
{
}

void PeriodicTimer::start(boost::asio::io_context& context, std::chrono::steady_clock::duration initialInterval)
{
    mTimer = std::make_unique<boost::asio::steady_timer>(context, initialInterval);
    mTimer->async_wait(
        [this](auto errCode){timerHandlerInternal(errCode);});
}

PeriodicTimer::~PeriodicTimer() = default;

void PeriodicTimer::timerHandlerInternal(const boost::system::error_code& errorCode)
{
    // The timer is canceled when either the object has been destroyed (and the timer task has not been properly deleted)
    // or if the underlying io_context is shut down. In either case exit before accessing any of the object's state because
    // its memory may have already been freed.
    if (errorCode.failed())
    {
        TVLOG(0) << "timerHandler called with error (" << errorCode.value() << "): " << errorCode.message();
        return;
    }

    timerHandler();
    if (mInterval != decltype(mInterval){})
    {
        auto now = std::chrono::steady_clock::now();
        auto newExpire = mTimer->expiry() + mInterval;
        if (newExpire < now)
        {
            TLOG(WARNING) << "Periodic timer skipped interval.";
            newExpire = now + mInterval;
        }
        mTimer->expires_at(newExpire);
        mTimer->async_wait(
            [this](auto errCode){timerHandlerInternal(errCode);});
    }
}

OneShotTimer::OneShotTimer()
    : PeriodicTimer({})
{
}
