/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/Timer.hxx"

#include "erp/util/TLog.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TerminationHandler.hxx"

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/system_timer.hpp>


class Timer::TimerJob : public boost::asio::system_timer
{
public:
    using boost::asio::system_timer::system_timer;
};


Timer::Timer (void)
    : mMutex(),
      mNextToken(NotAJob + 1),
      mIoContext(),
      mWork(mIoContext), // Prevent exiting mIoContext's run method unless mIoContext is explicitly shut down.
      mTimerThread([&]{mIoContext.run();}),
      mJobMap()
{
    ThreadNames::instance().setThreadName(mTimerThread.get_id(), "timer");
    TerminationHandler::instance().registerCallback(
        [this](bool){shutDown();});
}


Timer::~Timer (void)
{
    shutDown();
}


Timer::JobToken Timer::runAt (std::chrono::system_clock::time_point absoluteTime, Job&& job)
{
    auto outerJob = std::make_shared<TimerJob>(mIoContext, absoluteTime);
    const auto token = addJob(outerJob);
    outerJob->async_wait(
        [this, job=std::move(job), outerJob, token]
        (const boost::system::error_code& error)
        {
            if ( ! error)
            {
                job();
            }
            else
            {
                TLOG(ERROR) << "error during Timer::runAt: " << error;
            }
            removeJob(token);
        });
    return token;
}


Timer::JobToken Timer::runIn (std::chrono::system_clock::duration relativeTime, Job&& job)
{
    return runAt(relativeTime + std::chrono::system_clock::now(), std::move(job));
}


void Timer::runJobAndRepeat (
    Timer::Job&& innerJob,
    std::shared_ptr<TimerJob> outerJob,
    const std::chrono::system_clock::duration repeatingDelay,
    const Timer::JobToken token,
    const boost::system::error_code& error)
{
    if (error)
    {
        removeJob(token);
        return;
    }

    innerJob();

    // Schedule another run.
    Expect(outerJob != nullptr, "outerJob is empty");
    outerJob->expires_at(outerJob->expires_at() + repeatingDelay);
    outerJob->async_wait(
        [this, innerJob=std::move(innerJob), outerJob, repeatingDelay, token]
            (const boost::system::error_code& error) mutable
        {
            runJobAndRepeat(std::move(innerJob), outerJob, repeatingDelay, token, error);
        });
}


Timer::JobToken Timer::runRepeating (
    std::chrono::system_clock::duration initialDelay,
    std::chrono::system_clock::duration repeatingDelay,
    Job&& innerJob)
{
    auto outerJob = std::make_shared<TimerJob>(mIoContext,std::chrono::system_clock::now() + initialDelay);
    const auto token = addJob(outerJob);
    outerJob->async_wait(
        [this, innerJob=std::move(innerJob), outerJob, repeatingDelay, token]
        (const boost::system::error_code& error) mutable
        {
            runJobAndRepeat(std::move(innerJob), outerJob, repeatingDelay, token, error);
        });
    return token;
}


void Timer::shutDown (void)
{
    if ( ! mIoContext.stopped())
        mIoContext.stop();
    if (mTimerThread.joinable())
        mTimerThread.join();
}


Timer::JobToken Timer::addJob (std::shared_ptr<TimerJob> job)
{
    std::lock_guard lock (mMutex);

    const bool success = mJobMap.emplace(mNextToken, job).second;
    Expect(success, "failed to add timer job");

    return mNextToken++;
}


void Timer::removeJob (JobToken token)
{
    std::lock_guard lock (mMutex);

    auto iterator = mJobMap.find(token);
    if (iterator != mJobMap.end())
        mJobMap.erase(iterator);
    else
    {
        // Not finding the job is not a problem in itself but hints at a bug in the caller's code.
        TLOG(WARNING) << "can't find and remove timer job";
    }
}


void Timer::cancel (const JobToken token)
{
    if (token == NotAJob)
        return; // There is nothing to do.

    std::shared_ptr<TimerJob> job;

    {
        std::lock_guard lock (mMutex);

        auto iterator = mJobMap.find(token);
        if (iterator != mJobMap.end())
            job = iterator->second;
    }

    if (job != nullptr)
        job->cancel();
    else
    {
        // Not finding the job is not a problem in itself but hints at a bug in the caller's code.
        TLOG(WARNING) << "can't find and remove timer job";
    }
}
