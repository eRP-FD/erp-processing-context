/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TIMER_HXX
#define ERP_PROCESSING_CONTEXT_TIMER_HXX

#include <boost/asio/io_context.hpp>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>


/**
 * Another timer class.
 * This one manages an arbitrary number of jobs that are to be executed at specific times, relative or absolute, in the
 * future.
 *
 * The implementation is based on boost asio and uses one thread for all timer jobs.
 */
class Timer
{
public:
    Timer (void);
    ~Timer (void);

    using Job = std::function<void(void)>;

    using JobToken = uint64_t;
    constexpr static JobToken NotAJob = 0;

    JobToken runAt (std::chrono::system_clock::time_point absoluteTime, Job&& job);
    JobToken runIn (std::chrono::system_clock::duration relativeTime, Job&& job);
    JobToken runRepeating (
        std::chrono::system_clock::duration initialDelay,
        std::chrono::system_clock::duration repeatingDelay,
        Job&& job);

    void cancel (JobToken token);

private:
    std::mutex mMutex;
    JobToken mNextToken;
    boost::asio::io_context mIoContext;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> mWork;
    std::thread mTimerThread;
    class TimerJob;
    std::unordered_map<JobToken,std::shared_ptr<TimerJob>> mJobMap;

    void runJobAndRepeat (
        Timer::Job&& innerJob,
        std::shared_ptr<TimerJob> outerJob,
        const std::chrono::system_clock::duration repeatingDelay,
        const Timer::JobToken token,
        const boost::system::error_code& error);
    void shutDown (void);
    JobToken addJob (std::shared_ptr<TimerJob> job);
    void removeJob (JobToken token);
};


#endif
