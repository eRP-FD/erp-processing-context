/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/hsm/TeeTokenUpdater.hxx"

#include "erp/hsm/BlobCache.hxx"
#include "erp/hsm/HsmClient.hxx"
#include "erp/hsm/HsmFactory.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/hsm/HsmSession.hxx"

#include "erp/util/TLog.hxx"
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
