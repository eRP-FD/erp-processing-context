/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "shared/hsm/production/HsmProductionClient.hxx"
#include "shared/hsm/production/HsmProductionFactory.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/TLog.hxx"
#include "test/mock/MockBlobDatabase.hxx"

#include <gtest/gtest.h>


class HsmProductionFactoryTest : public testing::Test
{
public:
    void skipIfUnsupported (void)
    {
#ifdef __APPLE__
        GTEST_SKIP():
#endif
        auto device = Configuration::instance().getStringValue(ConfigurationKey::HSM_DEVICE);
        if (device.empty())
            GTEST_SKIP();
        else
            TVLOG(1) << "will connect to HSM with " << device;
    }
};


TEST_F(HsmProductionFactoryTest, DISABLED_logIn)
{
    skipIfUnsupported();

    auto blobCache = MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::SimulatedHsm);
    HsmProductionFactory factory (
        std::make_unique<HsmProductionClient>(),
        std::move(blobCache));

    auto session = factory.connect();
    ASSERT_NE(session, nullptr);
}
