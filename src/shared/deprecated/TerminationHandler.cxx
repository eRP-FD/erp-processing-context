/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/deprecated/TerminationHandler.hxx"

#include "shared/deprecated/SignalHandler.hxx"
#include "shared/util/TLog.hxx"
#include "shared/util/ThreadNames.hxx"
#include "shared/util/Expect.hxx"

#include <iostream>
#include <memory>

TerminationHandler::~TerminationHandler() = default;

std::unique_ptr<TerminationHandler>& TerminationHandler::rawInstance (void)
{
    class LocalTerminationHandler : public TerminationHandler {}; // Provide a public constructor for make_unique
    static std::unique_ptr<TerminationHandler> instance = std::make_unique<LocalTerminationHandler>();
    return instance;
}

TerminationHandler::TerminationHandler(void)
    : mMutex()
    , mCallbacks()
    , mState(State::Running)
    , mHasError(false)
{
}


std::optional<size_t> TerminationHandler::registerCallback (std::function<void(bool hasError)>&& callback)
{
    if (getState() != State::Running)
    {
        TLOG(WARNING) << "application is terminating, registering another termination callback is not possible anymore";
        return {};
    }
    else
    {
        std::lock_guard lock (mMutex);
        mCallbacks.emplace_back(std::move(callback));
        return mCallbacks.size()-1;
    }
}


void TerminationHandler::notifyTerminationCallbacks(bool hasError)
{
    TVLOG(1) << "TerminationHandler::notifyTerminationCallbacks(" << hasError << ") called";

    {
        // Remember if there was an error that lead to termination. Even if this is a subsequent termination request
        // that will be ignored.
        mHasError = mHasError || hasError;
    }

    if (setState(State::Terminating) == State::Terminated)
    {
        // This is another request for termination. That should not occur. But as throwing an exception in this
        // situation would not help anyone and would probably terminate the application hard (with crash to desktop)
        // we only print an error message.
        TLOG(INFO) << "second termination request ignored";
        return;
    }
    callTerminationCallbacks();
}

void TerminationHandler::terminate()
{
    SignalHandler::manualTermination();
}

void TerminationHandler::gracefulShutdown(int delaySeconds)
{
    setState(State::Terminating);
    if (delaySeconds <= 0)
    {
        SignalHandler::gracefulShutdown();
    }
    // If delaySeconds > 0 the caller (BaseServiceContext::startShutdownTimers) owns the
    // timers and will call SignalHandler::gracefulShutdown() when the delay expires.
}

TerminationHandler::State TerminationHandler::setState (const State newState)
{
    State oldState = mState;

    if (oldState != newState)
    {
        mState = newState;
    }

    return oldState;
}


TerminationHandler::State TerminationHandler::getState (void) const
{
    return mState;
}


bool TerminationHandler::hasError (void) const
{
    return mHasError;
}


void TerminationHandler::callTerminationCallbacks (void)
{
    decltype(mCallbacks) callbacks;
    {
        std::unique_lock lock (mMutex);
        callbacks.swap(mCallbacks);
    }

    TVLOG(1) << "calling " << callbacks.size() << " termination handlers";
    for (const auto& callback : callbacks)
    {
        try
        {
            callback(mHasError);
        }
        catch(const std::exception& e)
        {
            TLOG(WARNING) << "one of the termination callbacks has thrown an exception, which was ignored";
            TVLOG(1) << "the exception was: " << e.what();
        }
        catch(...)
        {
            // Make sure that no exception leaves this handler. In case it runs as reaction to a signal handler the
            // application is already in an undefined state and what we do here is a matter of best-effort.
            // Letting an exception escape would likely result in a hard crash to desktop and certainly in following
            // callbacks not being run.
            TLOG(WARNING) << "one of the termination callbacks has thrown an exception, which was ignored";
        }
    }
    TVLOG(1) << "called all " << callbacks.size() << " termination handlers";

    setState(State::Terminated);
}


void TerminationHandler::setRawInstance (std::unique_ptr<TerminationHandler>&& newInstance)
{
    rawInstance() = std::move(newInstance);
}


bool TerminationHandler::isShuttingDown() const
{
    switch(mState.load())
    {
        case State::Running:
            return false;
        case State::Terminating:
        case State::Terminated:
            return true;
    }
    Fail("TerminationHandler invalid state: " + std::to_string(static_cast<uintmax_t>(mState.load())));
}
