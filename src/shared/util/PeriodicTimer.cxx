/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "PeriodicTimer.hxx"
#include "shared/util/Demangle.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/TLog.hxx"
#include "shared/util/DurationConsumer.hxx"

TimerHandler::TimerHandler(std::string name)
    : mName{std::move(name)}
{
}

const std::string& TimerHandler::timerName() const
{
    return mName;
}

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
        // according to the documentation only `operation_aborted` should occur.
        // see: https://www.boost.org/doc/libs/latest/doc/html/boost_asio/reference/basic_waitable_timer/async_wait.html
        TLOG(ERROR) << handler->timerName() << ": timerHandler called with error (" << errorCode.value()
                    << "): " << errorCode.message();
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
            // Set up the duration consumer that will log spend time
            DurationConsumerGuard durationConsumerGuard("PeriodicTimer");
            handler->timerHandler();
        }
        catch (const std::exception& e)
        {
            JsonLog log{LogId::INTERNAL_ERROR};
            log.valuesFromException(e);
            log.keyValue("timer", handler->timerName());
            log.message("TimerHandler threw an exception");
        }
        catch (...)
        {
            JsonLog log{LogId::INTERNAL_ERROR};
            log.keyValue("timer", handler->timerName());
            log.message("TimerHandler threw an unknown object");
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
            JsonLog log{LogId::INTERNAL_WARNING};
            log.message("Periodic timer skipped interval.");
            log.keyValue("timer", handler->timerName());
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

FixedIntervalHandler::FixedIntervalHandler(std::string name, std::chrono::steady_clock::duration interval)
    : TimerHandler{std::move(name)}
    , mInterval{interval}
{
}

std::optional<std::chrono::steady_clock::duration> FixedIntervalHandler::nextInterval() const
{
    return mInterval;
}
