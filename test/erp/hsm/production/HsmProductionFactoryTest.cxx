#include "mock/hsm/MockBlobCache.hxx"

#include "erp/hsm/production/HsmProductionClient.hxx"
#include "erp/hsm/production/HsmProductionFactory.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/TLog.hxx"

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

    auto blobCache = MockBlobCache::createBlobCache(MockBlobCache::MockTarget::SimulatedHsm);
    HsmProductionFactory factory (
        std::make_unique<HsmProductionClient>(),
        std::move(blobCache));

    auto session = factory.connect();
    ASSERT_NE(session, nullptr);
}
