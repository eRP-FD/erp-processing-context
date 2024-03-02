/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/tsl/TslManager.hxx"
#include "erp/tsl/TslRefreshJob.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/Hash.hxx"
#include "test/erp/tsl/RefreshJobTestTslManager.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "mock/tsl/UrlRequestSenderMock.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/ResourceManager.hxx"

#include <memory>
#include <gtest/gtest.h>


class TslRefreshJobTest : public testing::Test
{
public:

    void SetUp() override
    {
        environmentGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der"));
        manager = TslTestHelper::createTslManager<RefreshJobTestTslManager>();
    }

    void TearDown() override
    {
        manager.reset();
        environmentGuard.reset();
    }

    std::unique_ptr<EnvironmentVariableGuard> environmentGuard;
    std::shared_ptr<RefreshJobTestTslManager> manager;
};


TEST_F(TslRefreshJobTest, triggerRefreshSuccess)
{
    TslRefreshJob refreshJob(*manager, std::chrono::milliseconds(10));

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
