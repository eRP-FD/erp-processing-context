/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "PeriodicTimer.hxx"
#include "erp/util/Demangle.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"


PeriodicTimerBase::PeriodicTimerBase(std::shared_ptr<TimerHandler> timerHandler)
    : mTimerHandler(std::move(timerHandler))
{
}

auto PeriodicTimerBase::timerCallback(const HandlerSharedPtr& handler)
{
    return [this, weakHandler = std::weak_ptr{handler}](const boost::system::error_code& code) mutable {
        auto lockedHandler = weakHandler.lock();
        if (lockedHandler)
        {
            timerHandlerInternal(lockedHandler, code);
        }
    };
}


void PeriodicTimerBase::start(boost::asio::io_context& context, std::chrono::steady_clock::duration initialInterval)
{
    if (! mTimer)
    {
        mTimer = std::make_unique<boost::asio::steady_timer>(context, initialInterval);
    }
    else
    {
        mTimer->cancel();
    }
    mTimer->async_wait(timerCallback(mTimerHandler));
}


void PeriodicTimerBase::cancel()
{
    if (mTimer)
        mTimer->cancel();
}

PeriodicTimerBase::~PeriodicTimerBase()
{
    // there is a combination of atomic and std::mutex
    // first acquire lock to make sure the handler isn't running
    mTimerHandler->mMutex.lock();
    mTimerHandler->canceled = true;
    // keep a weak pointer to the Handler to allow checking if it has been deleted by calling reset
    std::weak_ptr weakHandler{mTimerHandler};
    mTimerHandler.reset();
    auto lockedHandler = weakHandler.lock();
    if (lockedHandler)
    {
        // Handler has not been deleted, therefore we can unlock the mutex to avoid deadlock in Timer callback
        lockedHandler->mMutex.unlock();
    }
}

void PeriodicTimerBase::timerHandlerInternal(const HandlerSharedPtr& handler,
                                             const boost::system::error_code& errorCode)
{

    // The timer is canceled when either the object has been destroyed (and the timer task has not been properly deleted)
    // or if the underlying io_context is shut down. In either case exit before accessing any of the object's state because
    // its memory may have already been freed.
    if (errorCode == boost::asio::error::operation_aborted)
    {
        return;
    }
    if (errorCode.failed())
    {
        TVLOG(0) << "timerHandler called with error (" << errorCode.value() << "): " << errorCode.message();
        return;
    }

    std::optional<std::chrono::steady_clock::duration> interval;

    {
        std::lock_guard guard{handler->mMutex};
        if (handler->canceled)
        {
            return;
        }
        try
        {
            handler->timerHandler();
        }
        catch (const std::exception& e)
        {
            auto& handlerRef = *handler.get();
            TLOG(ERROR) << "TimerHandler threw an exception of type " << util::demangle(typeid(e).name())
                        << " from Timer " << util::demangle(typeid(handlerRef).name());
            throw;
        }
        interval = handler->nextInterval();
        if (!interval)
        {
            handler->canceled = true;
        }
    }

    if (interval)
    {
        auto now = std::chrono::steady_clock::now();
        auto newExpire = mTimer->expiry() + *interval;
        if (newExpire < now)
        {
            TLOG(WARNING) << "Periodic timer skipped interval.";
            newExpire = now + *interval;
        }
        mTimer->expires_at(newExpire);
        mTimer->async_wait(timerCallback(handler));
    }
}

std::optional<std::chrono::_V2::steady_clock::duration> OneShotHandler::nextInterval() const
{
    return std::nullopt;
}

FixedIntervalHandler::FixedIntervalHandler(std::chrono::steady_clock::duration interval)
    : mInterval{interval}
{
}

std::optional<std::chrono::steady_clock::duration> FixedIntervalHandler::nextInterval() const
{
    return mInterval;
}
