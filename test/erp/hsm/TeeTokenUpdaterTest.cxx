/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/hsm/TeeTokenUpdater.hxx"

#include "shared/hsm/BlobCache.hxx"
#include "shared/hsm/HsmClient.hxx"
#include "shared/hsm/HsmFactory.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/hsm/HsmSession.hxx"

#include "shared/util/TLog.hxx"
#include "test/util/BlobDatabaseHelper.hxx"
#include "test/util/HsmTestBase.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>


class TeeTokenUpdaterTest : public testing::Test,
                            public HsmTestBase
{
public:
    void SetUp (void) override
    {
        BlobDatabaseHelper::removeUnreferencedBlobs();
    }

    void TearDown() override
    {
        BlobDatabaseHelper::removeUnreferencedBlobs();
    }
};


TEST_F(TeeTokenUpdaterTest, doUpdateCalled)
{
    setupHsmTest(true, std::chrono::minutes(20), std::chrono::minutes(1));
    bool doUpdateCalled = false;

    mUpdateCallback = [&]() {
        doUpdateCalled = true;
    };
    mHsmPool->getTokenUpdater().update();

    ASSERT_TRUE(doUpdateCalled);
    mUpdateCallback = nullptr;
}


TEST_F(TeeTokenUpdaterTest, successfulRenewal)
{
    #ifdef _WIN32
    GTEST_SKIP();
    #endif

    // Do not allow production updater to keep the update cost minimal
    setupHsmTest(false, std::chrono::milliseconds(10), std::chrono::minutes(1));

    std::atomic_size_t updateCount = 0;

    mUpdateCallback = [&]() {
        ++updateCount;
    };

    mHsmPool->getTokenUpdater().requestUpdate();

    TVLOG(1) << "waiting for updates";
    testutils::waitFor([&updateCount]{return updateCount >= 2;});
    mUpdateCallback = nullptr;
}
