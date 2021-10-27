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

#include <gtest/gtest.h>


namespace {
    class RestartableTimer : public Timer
    {
    public:
        static void restartTimer (void) {Timer::restart();}
    };
}



class TeeTokenUpdaterTest : public testing::Test,
                            public HsmTestBase
{
public:
    static void SetUpTestSuite (void)
    {
        RestartableTimer::restartTimer();
    }

    virtual void SetUp (void) override
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
        [&](ErpBlob&& teeToken)
        {
            teeToken = std::move(teeToken);
            ++updateCount;
        },
        *mHsmFactory,
        false, // Do not allow production updater to keep the update cost minimal
        std::chrono::milliseconds(10),
        std::chrono::minutes(1));
    updater->requestUpdate();

    TVLOG(1) << "waiting for updates";

    // Wait long enough so that the first few updates are executed.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    TVLOG(1) << "woke up, counting updates";

    // 50ms / 10ms == 5 but due to external influences the 10ms intervals are usually not super-exact.
    ASSERT_GT(updateCount, 2);
}
