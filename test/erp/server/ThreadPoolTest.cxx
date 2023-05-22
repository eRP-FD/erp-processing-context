/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/server/ThreadPool.hxx"

#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>
#include <set>

#include "erp/util/TLog.hxx"


TEST(ThreadPoolTest, runOnAllThreads)//NOLINT(readability-function-cognitive-complexity)
{
    static constexpr size_t workers = 10;
    ThreadPool threadPool;
    // There is no HTTPS server whose socket connection would keep the io_context busy and the threads alive.
    // Therefore we have to assign a work object to achieve the same effect.
    boost::asio::io_context::work keepThreadsAlive(threadPool.ioContext());

    threadPool.setUp(workers, "test");
    //wait until all threads have started:
    ASSERT_NO_FATAL_FAILURE(testutils::waitFor([&]{return threadPool.getWorkerCount() >= workers;}));

    std::mutex idsMutex;
    std::set<std::thread::id> ids;
    std::atomic_size_t callCount = 0;

    threadPool.runOnAllThreads([&]{
        ++callCount;
        std::lock_guard lock{idsMutex};
        ids.insert(std::this_thread::get_id());
    });

    ASSERT_NO_FATAL_FAILURE(testutils::waitFor([&]{return callCount >= workers; }););

    threadPool.shutDown();
    EXPECT_EQ(callCount, workers);
    EXPECT_EQ(ids.size(), callCount) << "function was not called on different threads";
}
