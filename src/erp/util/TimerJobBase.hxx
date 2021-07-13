#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_UTIL_TIMERJOBBASE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_UTIL_TIMERJOBBASE_HXX

#include <boost/noncopyable.hpp>
#include <mutex>
#include <future>
#include <thread>

/**
 * This class is intended to be used as a base class for implementations
 * that have to be triggered by the timer and are intended to have own thread.
 * After we have a io_context solution that could be used for timers,
 * for the cases, when constantly running thread is not necessary,
 * PeriodicTimer would be a better approach.
 */
class TimerJobBase : private boost::noncopyable
{
public:
    TimerJobBase(TimerJobBase&& other) = delete;
    virtual ~TimerJobBase() noexcept;

    /**
     * Start the timer cycle in a dedicated thread. The cycle triggers the abstract method executeJob() with configured
     * specified interval. The time interval will be measured between end of an iteration and start of the next iteration.
     * This method returns immediately but the cycle continues until the TerminationHandler callback is called.
     */
    virtual void start (void);

    // Stop and join cycle thread.
    virtual void shutdown();

protected:
    /**
     * @param jobName               the name that should be used in job logs
     * @param interval              the interval between iterations in cycle
     */
    TimerJobBase(
        std::string jobName,
        const std::chrono::steady_clock::duration interval);

    /**
     * The implementation that is called just before
     * the first iteration in the cycle thread is started.
     *
     * It must not throw any exception, otherwise it would trigger application termination.
     */
    virtual void onStart (void) = 0;

    /**
     * Job implementation, it will be triggered with specified interval.
     *
     * It must not throw any exception, otherwise it would trigger application termination.
     */
    virtual void executeJob(void) = 0;

    /**
     * The implementation that is called on cycle thread finish.
     *
     * It must not throw any exception, otherwise it would trigger application termination.
     */
    virtual void onFinish (void) = 0;

    /**
     * Returns true if the cycle is aborted, false - otherwise.
     */
    bool isAborted (void) const;

    /**
     * Wait for the given duration or until mIsAborted==true.
     */
    void waitFor (std::chrono::steady_clock::duration interval);
private:
    mutable std::mutex mMutex;

    const std::string mJobName;
    const std::chrono::steady_clock::duration mInterval;

    std::thread mJobThread;
    std::condition_variable mAbortCondition;
    bool mIsAborted;
    bool mIsThreadExited;


    /**
     * Set the mAborted flag to true and notify mAbortCondition.
     * As a result cycle thread will leave its sending loop and become joinable.
     */
    void abort (void);



    void run ();


    /**
     * Wait until the cycle thread is exited.
     */
    void waitUntilThreadExit (void);
};


#endif
