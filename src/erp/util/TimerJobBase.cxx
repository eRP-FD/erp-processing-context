#include <chrono>
#include <functional>
#include <iostream>

#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/TerminationHandler.hxx"
#include "erp/util/TimerJobBase.hxx"


TimerJobBase::TimerJobBase (
    std::string jobName,
    const std::chrono::steady_clock::duration interval)
    : mMutex()
    , mJobName(std::move(jobName))
    , mInterval(interval)
    , mJobThread()
    , mAbortCondition()
    , mIsAborted(false)
    , mIsThreadExited(true)
{
}


void TimerJobBase::start (void)
{
    mIsThreadExited = false;
    mJobThread = std::thread(
        [this]
        {
            run();
        });

    TerminationHandler::instance().registerCallback(
        [this]
            (auto) mutable // Ignore the error flag
        {
          // Not using TVLOG because the logging framework may have already been shut down.
          std::cerr << "joining " << mJobName << " thread" << std::endl;
          shutdown();
          std::cerr << "joined " << mJobName << " thread" << std::endl;
        });
}


TimerJobBase::~TimerJobBase() noexcept
{
    if(mJobThread.joinable())
        mJobThread.join();
}


void TimerJobBase::abort (void)
{
    {
        std::lock_guard lock (mMutex);
        mIsAborted = true;
    }
    mAbortCondition.notify_all();
}


bool TimerJobBase::isAborted (void) const
{
    std::lock_guard lock (mMutex);
    return mIsAborted;
}


void TimerJobBase::shutdown()
{
    abort();

    // Wait for the thread to exit. Only then it is safe for us to lock the mutex again and continue with joining
    // the thread.
    waitUntilThreadExit();

    // Note that we expect that the thread is joinable, i.e has not been joined before, but in order to avoid
    // throwing an exception when possibly reacting to a signal, we don't treat that as error.
    std::lock_guard lock (mMutex);
    if (mJobThread.joinable())
    {
        mJobThread.join();
    }
}


void TimerJobBase::run (void)
{
    ThreadNames::instance().setCurrentThreadName(mJobName);

    try
    {
        onStart();

        while ( ! isAborted())
        {
            executeJob();
            waitFor(mInterval);
        }

        // We have to improve the TerminationHandler so that we can distinguish between orderly termination
        // and an abnormal termination.
        onFinish();
    }
    catch(const std::exception& e)
    {
        LOG(ERROR) << "caught an exception in the [" << mJobName << "] thread, terminating the application: " << e.what();
        TerminationHandler::instance().notifyTermination(true);
    }
    catch(...)
    {
        // Any exception that is caught here means that the proxy client is not accessible and therefore the
        // processing client can not function correctly. => terminate
        LOG(ERROR) << "caught an non-standard exception in the [" << mJobName << "] thread, terminating the application";
        TerminationHandler::instance().notifyTermination(true);
    }

    {
        std::lock_guard lock (mMutex);
        mIsThreadExited = true;
    }
    mAbortCondition.notify_all();

    TVLOG(0) << "leaving the " << mJobName << " thread";
}


void TimerJobBase::waitFor (const std::chrono::steady_clock::duration interval)
{
    std::unique_lock lock (mMutex);

    mAbortCondition.wait_for(
        lock,
        interval,
        [this]{return mIsAborted;});
}


void TimerJobBase::waitUntilThreadExit (void)
{
    std::unique_lock lock (mMutex);
    mAbortCondition.wait(
        lock,
        [this]{return mIsThreadExited;});
}
