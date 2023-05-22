/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/hsm/HsmPool.hxx"

#include "mock/hsm/HsmMockFactory.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/tools/HsmPoolHelper.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"

namespace hsmclient {
#include <hsmclient/ERP_Client.h>
}
#include <atomic>
#include <gtest/gtest.h>
#include <thread>


class HsmPoolTest : public testing::Test
{
public:
    HsmPoolTest (void)
        : hsmPool(
              std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(),
                                               MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
              TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>())
    {
    }

    HsmPool hsmPool;
};


TEST_F(HsmPoolTest, poolAcquire)//NOLINT(readability-function-cognitive-complexity)
{
    ASSERT_EQ(hsmPool.activeSessionCount(), 0);
    ASSERT_EQ(hsmPool.inactiveSessionCount(), 0);

    {
        const auto session = hsmPool.acquire();

        ASSERT_EQ(hsmPool.activeSessionCount(), 1);
        ASSERT_EQ(hsmPool.inactiveSessionCount(), 0);
    }

    ASSERT_EQ(hsmPool.activeSessionCount(), 0);
    ASSERT_EQ(hsmPool.inactiveSessionCount(), 1);
}


TEST_F(HsmPoolTest, poolAcquire_thread)//NOLINT(readability-function-cognitive-complexity)
{
    ASSERT_EQ(hsmPool.activeSessionCount(), 0);
    ASSERT_EQ(hsmPool.inactiveSessionCount(), 0);

    std::vector<std::thread> threads;

    constexpr size_t threadCount = 25;
    constexpr size_t jobCount = 250;

    hsmPool.resetMaxUsedSessionCount();

    for (size_t index=0; index<threadCount; ++index)
    {
        threads.emplace_back(
#if defined(_MSC_VER)
            // When compiling on Windows, MSVC complains that jobCount cannot be captured implicitly as there is no standard acquisition mode defined.
            // When compiling on MAC, the compiler there complains that jobCount is not used in the lambda function - strange.
            // gcc on linux does neither complain if jobCount is bound or not.
            [this, jobCount]
#else
            [this]
#endif
            {
                for (size_t innerIndex=0; innerIndex<jobCount; ++innerIndex)
                {
                    // Acquire a session and release it immediately.
                    auto session = hsmPool.acquire();

                    // Simulate that the session object is used for something.
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        );
    }

    for (auto& thread : threads)
        thread.join();

    ASSERT_EQ(hsmPool.activeSessionCount(), 0);
    ASSERT_LE(hsmPool.maxUsedSessionCount(), 5); // 5 is the default maximum number of HSM sessions.

    // threadCount is the maximum number of sessions that could have been acquired at the same time.
    // But with the short live time of session objects in this test, it is unlikely that this maximum is actually reached.
    std::cerr << "used up to " << hsmPool.inactiveSessionCount() << " HSM sessions at the same time in " << threadCount << "threads\n";
    ASSERT_LE(hsmPool.inactiveSessionCount(), threadCount);
}


TEST_F(HsmPoolTest, releasePool_onlyInactiveSessions)//NOLINT(readability-function-cognitive-complexity)
{
    ASSERT_EQ(hsmPool.activeSessionCount(), 0);
    ASSERT_EQ(hsmPool.inactiveSessionCount(), 0);

    {
        // Acquire a second session and release it immediately ...
        hsmPool.acquire();
    }

    // ... so that we have
    ASSERT_EQ(hsmPool.activeSessionCount(), 0);
    ASSERT_EQ(hsmPool.inactiveSessionCount(), 1);

    // Release the pool. This should be a) successful and b) drop the count of inactive sessions down to 0.
    hsmPool.releasePool();

    ASSERT_EQ(hsmPool.activeSessionCount(), 0);
    ASSERT_EQ(hsmPool.inactiveSessionCount(), 0);
}


TEST_F(HsmPoolTest, releasePool_failToAcquire)//NOLINT(readability-function-cognitive-complexity)
{
    hsmPool.releasePool();

    ASSERT_EQ(hsmPool.activeSessionCount(), 0);
    ASSERT_EQ(hsmPool.inactiveSessionCount(), 0);

    // Try to acquire a session. That is expected to fail for a released pool.
    ASSERT_ANY_THROW(hsmPool.acquire());
}


TEST_F(HsmPoolTest, releasePool_oneActiveSession)//NOLINT(readability-function-cognitive-complexity)
{
    ASSERT_EQ(hsmPool.activeSessionCount(), 0);
    ASSERT_EQ(hsmPool.inactiveSessionCount(), 0);

    {
        auto session = hsmPool.acquire();

        {
            // Acquire a second session and release it immediately ...
            hsmPool.acquire();
        }

        // ... so that we have
        ASSERT_EQ(hsmPool.activeSessionCount(), 1);
        ASSERT_EQ(hsmPool.inactiveSessionCount(), 1);

        // Release the pool. After that we should still have the one active session.
        hsmPool.releasePool();

        ASSERT_EQ(hsmPool.activeSessionCount(), 1);
        ASSERT_EQ(hsmPool.inactiveSessionCount(), 0);
    }

    // Note that releasing the pool cuts the connection of any active session with it. They are still able to
    // disconnect from the HSM but that is no longer recorded.
    ASSERT_EQ(hsmPool.activeSessionCount(), 1);
    ASSERT_EQ(hsmPool.inactiveSessionCount(), 0);
}

namespace {
    class HsmSessionTestClient : public HsmMockClient
    {
    public:
        std::atomic_size_t callCount = 0;
        ErpVector getRndBytes(const HsmRawSession&, size_t) override
        {
            ++callCount;
            return {};
        }
    };
}



TEST_F(HsmPoolTest, keepAlive)
{
    EnvironmentVariableGuard hsmKeepAliveInterval ("ERP_HSM_KEEP_ALIVE_INTERVAL_SECONDS", "1");

    auto client = std::make_unique<::HsmSessionTestClient>();
    auto& clientReference = *client;
    auto pool = HsmPool(
        std::make_unique<HsmMockFactory>(
            std::move(client),
            MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>());

    ASSERT_EQ(clientReference.callCount, 0);

    // Acquire one session. It is created on demand but not destroyed after use.
    {
        auto session = pool.acquire();
        // Use it to set the last HSM call time.
        session.session().getRandomData(1);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    ASSERT_GT(clientReference.callCount, 0);
}


TEST_F(HsmPoolTest, keepAliveJob)
{
    auto pool = HsmPool(
        std::make_unique<HsmMockFactory>(
            std::make_unique<::HsmSessionTestClient>(),
            MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>());

    ASSERT_TRUE(pool.isKeepAliveJobRunning());

    pool.releasePool();

    // Releasing the pool should also cancel the background job that keeps the HSM sessions alive.
    ASSERT_FALSE(pool.isKeepAliveJobRunning());
}
