/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/server/ThreadPool.hxx"
#include "util/RuntimeConfiguration.hxx"

#include <boost/asio/awaitable.hpp>


class MedicationExporterServiceContext;

/**
 * @brief A generic RunLoopScheduler class.
 *
 * The scheduler is used for managing the run loops of asynchronous processors.
 */
class RunLoopScheduler
{
public:
    enum class Reschedule : uint8_t
    {
        Delayed,
        Throttled,
        Immediate,
        TemporaryError
    };

    /**
     * @brief Constructs a new RunLoop instance and initializes its worker thread pool.
     *
     * This constructor sets up an internal ThreadPool that will be used to schedule
     * and execute coroutine workers. No threads are started at this point. The
     * RunLoopScheduler remains idle.

     * Threads are created later via `ThreadPool::setUp()`. The RunLoopScheduler remains idle until
     * `serve()` is called to spawn worker coroutines.
     */
    RunLoopScheduler();

    /**
     * @brief Starts the run loop by spawning worker coroutines on the thread pool.
     *
     * This method schedules the initial set of coroutine workers for the run loop.
     * Each worker is responsible for continuously processing events.
     *
     * Threads are not started here but later in the code (-> `joinAllThreads()`).
     *
     * @param serviceContext Shared pointer to the service context used by the workers.
     *                       A weak reference is passed to each coroutine to allow
     *                       graceful shutdown when the context expires.
     * @param worker         The actual runloop implementation.
     * @param workerCount    Number of worker coroutines to spawn. Typically this
     *                       corresponds to the number of threads configured in the
     *                       thread pool.
     */
    void
    serve(const std::shared_ptr<MedicationExporterServiceContext>& serviceContext,
          const std::function<boost::asio::awaitable<void>(RunLoopScheduler& scheduler,
                                                           std::weak_ptr<MedicationExporterServiceContext>)>& worker,
          size_t workerCount);

    /**
     * @brief Shut down the ThreadPool for graceful termination.
     */
    void shutDown();

    /**
     * @brief Test if the ThreadPool is running.
     *
     * @returns True when stopped.
     */
    bool isStopped() const;

    /**
     * @brief Get access to the ThreadPool (used for accessing the ioContext, actually).
     *
     * @returns ThreadPool& Reference to the ThreadPool.
     */
    ThreadPool& getThreadPool();

    bool checkIsPaused(const std::shared_ptr<MedicationExporterServiceContext>& serviceContext,
                       exporter::RuntimeConfiguration::ProcessorType processor);
    bool checkIsThrottled(const std::shared_ptr<MedicationExporterServiceContext>& serviceContext);

    std::chrono::milliseconds getThrottleValue() const
    {
        return mThrottleValue.load();
    }

private:
    ThreadPool mWorkerThreadPool;
    std::map<exporter::RuntimeConfiguration::ProcessorType, std::atomic_bool> mPaused;
    std::atomic<std::chrono::milliseconds> mThrottleValue;
};
