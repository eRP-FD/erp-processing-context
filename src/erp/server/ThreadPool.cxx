/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/server/ThreadPool.hxx"

#include "erp/server/Worker.hxx"
#include "erp/util/TLog.hxx"


#include <boost/asio.hpp>
#include <chrono>


ThreadPool::ThreadPool (void)
    : mThreads()
    , mIoContext(boost::asio::io_context(BOOST_ASIO_CONCURRENCY_HINT_SAFE))
{
}


ThreadPool::~ThreadPool (void)
{
    if ( ! mThreads.empty())
        shutDown();
}


void ThreadPool::setUp (
    const size_t threadCount)
{
    // Run the I/O service on the requested number of threads
    mThreads.reserve(threadCount);
    for(size_t index=0; index<threadCount; ++index)
    {
        mThreads.emplace_back(
            [this]{runWorkerOnThisThread();});
        ThreadNames::instance().setThreadName(mThreads.back().get_id(), "thread-" + std::to_string(index));
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
    mIoContext.stop();
    joinAllThreads();
}


void ThreadPool::joinAllThreads (void)
{
    for (auto& thread : mThreads)
    {
        thread.join();
    }
    mThreads.clear();

    TLOG(INFO) << "finished joining server threads";
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

