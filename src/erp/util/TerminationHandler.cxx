#include "erp/util/TerminationHandler.hxx"
#include "erp/util/ThreadNames.hxx"

#include <iostream>
#include <memory>


std::unique_ptr<TerminationHandler>& TerminationHandler::rawInstance (void)
{
    class LocalTerminationHandler : public TerminationHandler {}; // Provide a public constructor for make_unique
    static std::unique_ptr<TerminationHandler> instance = std::make_unique<LocalTerminationHandler>();
    return instance;
}


TerminationHandler::TerminationHandler (void)
    : mMutex(),
      mTerminationCondition(),
      mCallbacks(),
      mTerminationThread(),
      mState(State::Running),
      mHasError(false)
{
    // We will use mTerminationThread to call the termination callbacks, eventually. This allows other threads
    // to call the notifyTermination() method without having to worry about synchronous callbacks and locked mutexes.
    std::unique_lock lock (mMutex);
    mTerminationThread = std::make_unique<std::thread>(
        [this]
        {
            callTerminationCallbacks();
        });
}


TerminationHandler::~TerminationHandler (void)
{
    // The following can not prevent calling notifyTermination when it has been called before because we
    // cannot call notifyTermination with a locked mutex. But calling it a second time is not a technical problem, but
    // merely unelegant design.
    if (mState == State::Running)
        notifyTermination(false);

    mTerminationThread->join();
}


std::optional<size_t> TerminationHandler::registerCallback (std::function<void(bool hasError)>&& callback)
{
    if (getState() != State::Running)
    {
        std::cerr << "WARNING: application is terminating, registering another termination callback is not possible anymore" << std::endl;
        return {};
    }
    else
    {
        std::lock_guard lock (mMutex);
        mCallbacks.emplace_back(std::move(callback));
        return mCallbacks.size()-1;
    }
}


void TerminationHandler::notifyTermination (bool hasError)
{
    std::cerr << "TerminationHandler::notifyTermination(" << hasError << ") called" << std::endl;

    {
        // Remember if there was an error that lead to termination. Even if this is a subsequent termination request
        // that will be ignored.
        std::lock_guard lock (mMutex);
        mHasError |= hasError;
    }

    if (setState(State::Terminating) != State::Running)
    {
        // This is another request for termination. That should not occur. But as throwing an exception in this
        // situation would not help anyone and would probably terminate the application hard (with crash to desktop)
        // we only print an error message.
        std::cerr << "ERROR: second termination request ignored" << std::endl;
        return;
    }

    // Setting the state to Terminating triggers the calling of all callbacks in the `mTerminationThread`. In this
    // thread there remains nothing else to do.
}


void TerminationHandler::waitForTerminated (void)
{
    std::unique_lock lock (mMutex);
    if (mState != State::Terminated)
        mTerminationCondition.wait(
            lock,
            [this](){return mState==State::Terminated;});
}


TerminationHandler::State TerminationHandler::setState (const State newState)
{
    State oldState; // will be defined two lines down.
    {
        std::lock_guard lock (mMutex);
        oldState = mState;
    }

    if (oldState != newState)
    {
        {
            std::lock_guard lock(mMutex);
            mState = newState;
        }
        mTerminationCondition.notify_all();
    }

    return oldState;
}


TerminationHandler::State TerminationHandler::getState (void) const
{
    std::lock_guard lock (mMutex);
    return mState;
}


bool TerminationHandler::hasError (void) const
{
    std::lock_guard lock (mMutex);
    return mHasError;
}


void TerminationHandler::callTerminationCallbacks (void)
{
    {
        std::unique_lock lock(mMutex);
        ThreadNames::instance().setThreadName(mTerminationThread->get_id(), "termination");
    }

    std::vector<std::function<void(bool)>> callbacks;

    {
        // Wait for the state to change to Terminating.
        std::unique_lock lock (mMutex);
        mTerminationCondition.wait(
            lock,
            [this]{return mState==State::Terminating;});
        callbacks.swap(mCallbacks);
    }

    std::cerr << "    calling " << callbacks.size() << " termination handlers" << std::endl;
    for (const auto& callback : callbacks)
    {
        try
        {
            callback(mHasError);
        }
        catch(const std::exception& e)
        {
            std::cerr << "one of the termination callbacks has thrown an exception, which was ignored" << std::endl;
            std::cerr << "the exception was: " << e.what() << std::endl;
        }
        catch(...)
        {
            // Make sure that no exception leaves this handler. In case it runs as reaction to a signal handler the
            // application is already in an undefined state and what we do here is a matter of best-effort.
            // Letting an exception escape would likely result in a hard crash to desktop and certainly in following
            // callbacks not being run.
            std::cerr << "one of the termination callbacks has thrown an exception, which was ignored";
        }
    }
    std::cerr << "    called all " << callbacks.size() << " termination handlers" << std::endl;

    setState(State::Terminated);
}


void TerminationHandler::setRawInstance (std::unique_ptr<TerminationHandler>&& newInstance)
{
    rawInstance() = std::move(newInstance);
}
