/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/server/ThreadPool.hxx"

#include "shared/server/Worker.hxx"
#include "shared/util/TLog.hxx"


#include <boost/asio.hpp>
#include <chrono>


ThreadPool::ThreadPool (void)
    : mThreads()
    , mIoContext(boost::asio::io_context(BOOST_ASIO_CONCURRENCY_HINT_SAFE))
    , mWorkGuard(boost::asio::make_work_guard(mIoContext))
{
}


ThreadPool::~ThreadPool (void)
{
    if ( ! mThreads.empty())
        shutDown();
}


void ThreadPool::setUp(const size_t threadCount, std::string_view threadBaseName)
{
    // Run the I/O service on the requested number of threads
    mThreads.reserve(threadCount);
    for (size_t index = 0; index < threadCount; ++index)
    {
        mThreads.emplace_back([this] {
            runWorkerOnThisThread();
        });
        ThreadNames::instance().setThreadName(mThreads.back().get_id(),
                                              std::string(threadBaseName) + "-" + std::to_string(index));
    }
}

void ThreadPool::runWorkerOnThisThread()
{
    std::unique_lock lock{mWorkerListMutex};
    auto& worker = mWorkerList.emplace_back();
    lock.unlock();
    worker.run(mIoContext);
}


size_t ThreadPool::getThreadCount (void) const
{
    return mThreads.size();
}


void ThreadPool::runOnAllThreads(const std::function<void()>& function)
{
    std::shared_lock lock{mWorkerListMutex};
    for (auto& worker : mWorkerList)
    {
        worker.queueWork(std::function{function});
    }
}

void ThreadPool::shutDown (void)
{
    TVLOG(0) << "shutting down all server threads";
    mWorkGuard.reset();  // Allow io_context to exit after all work is done
    mIoContext.stop();
    joinAllThreads();
}


void ThreadPool::joinAllThreads (void)
{
    for (auto& thread : mThreads)
    {
        if (thread.joinable()) {
            thread.join();
        }
    }
    mThreads.clear();

    TVLOG(0) << "finished joining server threads";
}

boost::asio::io_context& ThreadPool::ioContext()
{
    return mIoContext;
}

const boost::asio::io_context& ThreadPool::ioContext() const
{
    return mIoContext;
}

size_t ThreadPool::getWorkerCount() const
{
    std::shared_lock lock{mWorkerListMutex};
    return mWorkerList.size();
}
