/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "MockAndProductionTestBase.hxx"
#include "shared/hsm/production/HsmProductionFactory.hxx"
#if WITH_HSM_TPM_PRODUCTION > 0
#include "shared/hsm/production/HsmProductionClient.hxx"
#include "shared/enrolment/EnrolmentHelper.hxx"
#endif

void MockAndProductionTestBase::SetUp()
{
    auto factories = StaticData::makeMockFactories();
    factories.blobCacheFactory = [] {
        return MockBlobDatabase::createBlobCache(GetParam());
    };
    switch (GetParam())
    {
        case MockBlobCache::MockTarget::SimulatedHsm: {
#if WITH_HSM_TPM_PRODUCTION > 0
            tpmFactory = TpmTestHelper::createProductionTpmFactory();
            factories.teeTokenUpdaterFactory = TeeTokenUpdater::createProductionTeeTokenUpdaterFactory();
            factories.tpmFactory = tpmFactory;
            factories.hsmClientFactory = []() {
                return std::make_unique<HsmProductionClient>();
            };
            factories.hsmFactoryFactory = [](std::unique_ptr<HsmClient>&& client,
                                             std::shared_ptr<BlobCache> blobCache) {
                return std::make_unique<HsmProductionFactory>(std::move(client), std::move(blobCache));
            };
            try
            {
                mSimulatedContext.emplace(Configuration::instance(), std::move(factories));
                mContext = &mSimulatedContext.value();
            }
            catch (const std::exception& e)
            {
                GTEST_SKIP_(e.what());
                return;
            }
            TLOG(INFO) << "initialized simulated context";
            // Run the tee token updater once, now, so that we have a valid Tee token.
            EnrolmentHelper::refreshTeeToken(mSimulatedContext->getHsmPool());
#else
            tpmFactory = [](BlobCache&){ return nullptr; };
            GTEST_SKIP_("Simulated HSM is disabled");
#endif
            break;
        }
        case MockBlobCache::MockTarget::MockedHsm: {
            tpmFactory = TpmTestHelper::createMockTpmFactory();
            factories.tpmFactory = tpmFactory;
            mMockedContext.emplace(Configuration::instance(), std::move(factories));
            mContext = &mMockedContext.value();
            break;
        }
    }
}