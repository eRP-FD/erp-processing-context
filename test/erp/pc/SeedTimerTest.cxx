/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/pc/SeedTimer.hxx"

#include "erp/hsm/HsmPool.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>

TEST(SeedTimerTest, distribute)//NOLINT(readability-function-cognitive-complexity)
{

    #ifndef _WIN32
    static constexpr size_t threads = 20;
    #else
    // On windows machines starting more than 1 thread may result in a memory overwrite
    // (starting more than 10 threads always results in a crash)
    // (see https://dth01.ibmgcloud.net/jira/browse/ERP-5838).
    static constexpr size_t threads = 1;
    #endif
    static constexpr std::chrono::milliseconds interval{200};
    ThreadPool pool;
    // There is no HTTPS server whose socket connection would keep the io_context busy and the threads alive.
    // Therefore we have to assign a work object to achieve the same effect.
    boost::asio::io_context::work keepThreadsAlive(pool.ioContext());
    pool.setUp(threads, "test");
    ASSERT_NO_FATAL_FAILURE(testutils::waitFor([&]{return pool.getWorkerCount() >= threads; }));
    std::atomic_size_t callCount = 0;
    HsmPool mockHsmPool{
        std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(),
                                         MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>()};
    SeedTimer seedTimer(pool, mockHsmPool, interval,
                        [&](const SafeString& seed){
                            ++callCount;
                            ASSERT_EQ(seed.size(), Seeder::seedBytes);
                        });
    seedTimer.start(pool.ioContext(), interval);
    EXPECT_EQ(callCount, 0);
    ASSERT_NO_FATAL_FAILURE(testutils::waitFor([&]{return callCount >= threads;}));
    pool.shutDown();
    EXPECT_EQ(callCount, threads);
}
