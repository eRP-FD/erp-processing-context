/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/CommonStaticData.hxx"

#include "mock/hsm/HsmMockClient.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/tpm/TpmMock.hxx"
#include "mock/tsl/MockTslManager.hxx"
#include "shared/hsm/production/ProductionVsdmKeyBlobDatabase.hxx"
#include "shared/server/BaseServiceContext.hxx"
#include "shared/server/handler/RequestHandlerManager.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockVsdmKeyBlobDatabase.hxx"

std::unique_ptr<BaseHttpsServer> CommonStaticData::nullHttpsServer(const std::string_view, uint16_t,
                                                                   RequestHandlerManager&&, BaseServiceContext&, bool,
                                                                   const SafeString&)
{
    return nullptr;
}


void CommonStaticData::fillMockBaseFactories(BaseFactories& baseFactories)
{
    baseFactories.blobCacheFactory = [] {
        return MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm);
    };
    baseFactories.hsmClientFactory = [] {
        return std::make_unique<HsmMockClient>();
    };
    baseFactories.hsmFactoryFactory = [](std::unique_ptr<HsmClient>&& client, std::shared_ptr<BlobCache> blobCache) {
        return std::make_unique<HsmMockFactory>(std::move(client), std::move(blobCache));
    };
    baseFactories.teeTokenUpdaterFactory = TeeTokenUpdater::createMockTeeTokenUpdaterFactory();

    baseFactories.tslManagerFactory = MockTslManager::createMockTslManager;

    baseFactories.vsdmKeyBlobDatabaseFactory = []() -> std::unique_ptr<VsdmKeyBlobDatabase> {
        if (TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
            return std::make_unique<ProductionVsdmKeyBlobDatabase>();
        else
            return std::make_unique<MockVsdmKeyBlobDatabase>();
    };

    baseFactories.tpmFactory = TpmMock::createFactory();
    baseFactories.xmlValidatorFactory = &CommonStaticData::getXmlValidator;


    baseFactories.enrolmentServerFactory = &CommonStaticData::nullHttpsServer;
    baseFactories.adminServerFactory = &CommonStaticData::nullHttpsServer;
}
