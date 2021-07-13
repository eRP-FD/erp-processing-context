#include "erp/tsl/TslManager.hxx"
#include "erp/tsl/TslRefreshJob.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/Hash.hxx"
#include "test/erp/tsl/RefreshJobTestTslManager.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/UrlRequestSenderMock.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/StaticData.hxx"

#include <memory>
#include <gtest/gtest.h>


class TslRefreshJobTest : public testing::Test
{
public:

    virtual void SetUp() override
    {
        environmentGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            std::string{TEST_DATA_DIR} + "/tsl/TslSignerCertificateIssuer.der");
        manager = TslTestHelper::createTslManager<RefreshJobTestTslManager>();
    }

    virtual void TearDown() override
    {
        manager.reset();
        environmentGuard.reset();
    }

    std::unique_ptr<EnvironmentVariableGuard> environmentGuard;
    std::shared_ptr<RefreshJobTestTslManager> manager;
};


TEST_F(TslRefreshJobTest, triggerRefreshSuccess)
{
    TslRefreshJob refreshJob(manager, std::chrono::milliseconds(10));

    refreshJob.start();

    // wait till the actions are triggered, but not longer than 10 seconds
    size_t counter = 0;
    while(counter < 100 && manager->updateCount.operator int() < 2)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        counter++;
    }

    EXPECT_TRUE(manager->updateCount.operator int() > 2);

    refreshJob.shutdown();
}
