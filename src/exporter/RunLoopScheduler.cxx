/*
* (C) Copyright IBM Deutschland GmbH 2021, 2025
* (C) Copyright IBM Corp. 2021, 2025
*
* non-exclusively licensed to gematik GmbH
*/

#include "exporter/RunLoopScheduler.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "exporter/util/RuntimeConfiguration.hxx"


RunLoopScheduler::RunLoopScheduler()
    : mWorkerThreadPool()
{
    for (auto processor : magic_enum::enum_values<exporter::RuntimeConfiguration::ProcessorType>())
    {
        mPaused[processor] = false;
    }
}

void RunLoopScheduler::serve(const std::shared_ptr<MedicationExporterServiceContext>& serviceContext,
                             const std::function<boost::asio::awaitable<void>(
                                 RunLoopScheduler& scheduler, std::weak_ptr<MedicationExporterServiceContext>)>& worker,
                             size_t workerCount)
{
    // Schedule workers (coroutines) to the io context.
    for (size_t i = 0; i < workerCount; ++i)
    {
        boost::asio::co_spawn(mWorkerThreadPool.ioContext(), worker(*this, std::weak_ptr{serviceContext}),
                              boost::asio::detached);
    }
}

void RunLoopScheduler::shutDown()
{
    mWorkerThreadPool.shutDown();
}

bool RunLoopScheduler::isStopped() const
{
    return mWorkerThreadPool.ioContext().stopped();
}

ThreadPool& RunLoopScheduler::getThreadPool()
{
    return mWorkerThreadPool;
}

bool RunLoopScheduler::checkIsPaused(const std::shared_ptr<MedicationExporterServiceContext>& serviceContext,
                                     const exporter::RuntimeConfiguration::ProcessorType processor)
{
    const auto paused = serviceContext->getRuntimeConfigurationGetter()->isPaused(processor);
    if (mPaused[processor].exchange(paused) != paused)
    {
        TLOG(INFO) << (paused ? "Paused" : "Resuming");
    }
    return paused;
}

bool RunLoopScheduler::checkIsThrottled(const std::shared_ptr<MedicationExporterServiceContext>& serviceContext)
{
    using namespace std::chrono_literals;
    const auto throttleValue = serviceContext->getRuntimeConfigurationGetter()->throttleValue();
    const bool throttlingActive = throttleValue > 0ms;
    if (mThrottleValue.exchange(throttleValue) != throttleValue)
    {
        if (throttlingActive)
        {
            TLOG(INFO) << "Throttling active, value: " << throttleValue.count() << "ms";
        }
        else
        {
            TLOG(INFO) << "Throttling inactive";
        }
    }
    return throttlingActive;
}
