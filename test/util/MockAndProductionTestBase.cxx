/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "MockAndProductionTestBase.hxx"
#include "erp/hsm/production/HsmProductionFactory.hxx"
#include "erp/hsm/production/HsmProductionClient.hxx"
#include "erp/enrolment/EnrolmentHelper.hxx"

void MockAndProductionTestBase::SetUp()
{
    auto factories = StaticData::makeMockFactories();
    factories.blobCacheFactory = [] {
        return MockBlobDatabase::createBlobCache(GetParam());
    };
    switch (GetParam())
    {
        case MockBlobCache::MockTarget::SimulatedHsm: {
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
            auto hsmFactory =
                std::make_unique<HsmProductionFactory>(std::make_unique<HsmProductionClient>(), mSimulatedContext->getBlobCache());
            // Run the tee token updater once, now, so that we have a valid Tee token.
            auto teeToken =
                EnrolmentHelper(HsmIdentity::getWorkIdentity()).createTeeToken(*mSimulatedContext->getBlobCache());
            auto pool = mSimulatedContext->getHsmPool().acquire();
            pool.session().setTeeToken(teeToken);

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