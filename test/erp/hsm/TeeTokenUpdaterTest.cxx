/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/TeeTokenUpdater.hxx"

#include "erp/hsm/BlobCache.hxx"
#include "erp/hsm/HsmClient.hxx"
#include "erp/hsm/HsmFactory.hxx"
#include "erp/hsm/HsmSession.hxx"

#include "erp/util/TLog.hxx"
#include "test/util/HsmTestBase.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>


class TeeTokenUpdaterTest : public testing::Test,
                            public HsmTestBase
{
public:
    void SetUp (void) override
    {
        setupHsmTest();
    }
};


TEST_F(TeeTokenUpdaterTest, doUpdateCalled)
{
    ErpBlob teeToken;
    bool doUpdateCalled = false;

    auto updater = createTeeTokenUpdater(
        [&](auto&&blob){teeToken = blob; doUpdateCalled=true;},
        *mHsmFactory,
        true,
        std::chrono::minutes(20),
        std::chrono::minutes(1));
    updater->update();

    ASSERT_TRUE(doUpdateCalled);
}


TEST_F(TeeTokenUpdaterTest, successfulRenewal)
{
    #ifdef _WIN32
    GTEST_SKIP();
    #endif

    ErpBlob teeToken;
    std::atomic_size_t updateCount = 0;

    auto updater = createTeeTokenUpdater(
        [&](ErpBlob&& teeToken1)
        {
            teeToken = std::move(teeToken1);
            ++updateCount;
        },
        *mHsmFactory,
        false, // Do not allow production updater to keep the update cost minimal
        std::chrono::milliseconds(10),
        std::chrono::minutes(1));
    updater->requestUpdate();

    TVLOG(1) << "waiting for updates";
    testutils::waitFor([&updateCount]{return updateCount >= 2;});
}
