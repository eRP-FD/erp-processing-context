/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <chrono>
#include <functional>
#include <iostream>

#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/TerminationHandler.hxx"
#include "erp/util/TimerJobBase.hxx"
#include "erp/util/DurationConsumer.hxx"


TimerJobBase::TimerJobBase (
    std::string jobName,
    const std::chrono::steady_clock::duration interval)
    : mMutex()
    , mJobName(std::move(jobName))
    , mInterval(interval)
    , mJobThread()
    , mAbortCondition()
    , mIsAborted(false)
{
}

std::chrono::steady_clock::duration TimerJobBase::getInterval()
{
    return mInterval;
}

void TimerJobBase::setInterval(const std::chrono::steady_clock::duration nextInterval)
{
    mInterval = nextInterval;
}

void TimerJobBase::start (void)
{
    mJobThread = std::thread(
        [this]
        {
            run();
        });

    TerminationHandler::instance().registerCallback(
        [this]
            (auto) mutable // Ignore the error flag
        {
          TVLOG(1) << "joining " << mJobName << " thread";
          shutdown();
          TVLOG(1) << "joined " << mJobName << " thread";
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
    if (mJobThread.joinable())
        mJobThread.join();
}


void TimerJobBase::run (void)
{
    ThreadNames::instance().setCurrentThreadName(mJobName);

    // Set up the duration consumer that will log spend time
    DurationConsumerGuard durationConsumerGuard(mJobName);

    try
    {
        onStart();

        // wait before the first job execution
        if ( ! isAborted())
        {
            waitFor(mInterval);
        }

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
        TLOG(ERROR) << "caught an exception in the [" << mJobName << "] thread, terminating the application: " << e.what();
        TerminationHandler::instance().terminate();
    }
    catch(...)
    {
        // Any exception that is caught here means that the proxy client is not accessible and therefore the
        // processing client can not function correctly. => terminate
        TLOG(ERROR) << "caught an non-standard exception in the [" << mJobName << "] thread, terminating the application";
        TerminationHandler::instance().terminate();
    }

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
