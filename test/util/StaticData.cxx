#include "test/util/StaticData.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/database/RedisClient.hxx"
#include "erp/hsm/production/ProductionVsdmKeyBlobDatabase.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/tpm/TpmMock.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/mock/MockVsdmKeyBlobDatabase.hxx"
#include "test/mock/RegistrationMock.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/TestConfiguration.hxx"
#include "mock/tsl/MockTslManager.hxx"


const Certificate StaticData::idpCertificate = Certificate::fromPem(R"(-----BEGIN CERTIFICATE-----
MIICsTCCAligAwIBAgIHAbssqQhqOzAKBggqhkjOPQQDAjCBhDELMAkGA1UEBhMC
REUxHzAdBgNVBAoMFmdlbWF0aWsgR21iSCBOT1QtVkFMSUQxMjAwBgNVBAsMKUtv
bXBvbmVudGVuLUNBIGRlciBUZWxlbWF0aWtpbmZyYXN0cnVrdHVyMSAwHgYDVQQD
DBdHRU0uS09NUC1DQTEwIFRFU1QtT05MWTAeFw0yMTAxMTUwMDAwMDBaFw0yNjAx
MTUyMzU5NTlaMEkxCzAJBgNVBAYTAkRFMSYwJAYDVQQKDB1nZW1hdGlrIFRFU1Qt
T05MWSAtIE5PVC1WQUxJRDESMBAGA1UEAwwJSURQIFNpZyAzMFowFAYHKoZIzj0C
AQYJKyQDAwIIAQEHA0IABIYZnwiGAn5QYOx43Z8MwaZLD3r/bz6BTcQO5pbeum6q
QzYD5dDCcriw/VNPPZCQzXQPg4StWyy5OOq9TogBEmOjge0wgeowDgYDVR0PAQH/
BAQDAgeAMC0GBSskCAMDBCQwIjAgMB4wHDAaMAwMCklEUC1EaWVuc3QwCgYIKoIU
AEwEggQwIQYDVR0gBBowGDAKBggqghQATASBSzAKBggqghQATASBIzAfBgNVHSME
GDAWgBQo8Pjmqch3zENF25qu1zqDrA4PqDA4BggrBgEFBQcBAQQsMCowKAYIKwYB
BQUHMAGGHGh0dHA6Ly9laGNhLmdlbWF0aWsuZGUvb2NzcC8wHQYDVR0OBBYEFC94
M9LgW44lNgoAbkPaomnLjS8/MAwGA1UdEwEB/wQCMAAwCgYIKoZIzj0EAwIDRwAw
RAIgCg4yZDWmyBirgxzawz/S8DJnRFKtYU/YGNlRc7+kBHcCIBuzba3GspqSmoP1
VwMeNNKNaLsgV8vMbDJb30aqaiX1
-----END CERTIFICATE-----)");

Factories StaticData::makeMockFactories()
{
    Factories factories;
    factories.databaseFactory = [](HsmPool& hsmPool, KeyDerivation& keyDerivation) -> std::unique_ptr<Database> {
        std::unique_ptr<DatabaseBackend> backend;
        if (TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
            backend = std::make_unique<PostgresBackend>();
        else
            backend = std::make_unique<MockDatabase>(hsmPool);
        return std::make_unique<DatabaseFrontend>(std::move(backend), hsmPool, keyDerivation);
    };
    factories.blobCacheFactory = [] {
        return MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm);
    };
    factories.hsmClientFactory = [] {
        return std::make_unique<HsmMockClient>();
    };
    factories.hsmFactoryFactory = [](std::unique_ptr<HsmClient>&& client, std::shared_ptr<BlobCache> blobCache) {
        return std::make_unique<HsmMockFactory>(std::move(client), std::move(blobCache));
    };
    factories.teeTokenUpdaterFactory = TeeTokenUpdater::createMockTeeTokenUpdaterFactory();

    factories.tslManagerFactory = MockTslManager::createMockTslManager;

    factories.redisClientFactory = [] {
        return TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_REDIS_MOCK, true)
                   ? std::unique_ptr<RedisInterface>(new MockRedisStore())
                   : std::unique_ptr<RedisInterface>(new RedisClient());
    };
    factories.vsdmKeyBlobDatabaseFactory = []() -> std::unique_ptr<VsdmKeyBlobDatabase> {
        if (TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
            return std::make_unique<ProductionVsdmKeyBlobDatabase>();
        else
            return std::make_unique<MockVsdmKeyBlobDatabase>();
    };

    factories.tpmFactory = TpmMock::createFactory();
    factories.incodeValidatorFactory = StaticData::getInCodeValidator;
    factories.xmlValidatorFactory = StaticData::getXmlValidator;
    factories.jsonValidatorFactory = StaticData::getJsonValidator;

    factories.teeServerFactory = [](const std::string_view, uint16_t, RequestHandlerManager&&, PcServiceContext&, bool,
                                    const SafeString&) {
        return std::unique_ptr<HttpsServer>{nullptr};
    };

    factories.enrolmentServerFactory = factories.teeServerFactory;
    factories.adminServerFactory = factories.teeServerFactory;

    return factories;
}

Factories StaticData::makeMockFactoriesWithServers()
{
    auto factories = makeMockFactories();
    factories.teeServerFactory = [](const std::string_view address, uint16_t port,
                                    RequestHandlerManager&& requestHandlers, PcServiceContext& serviceContext,
                                    bool enforceClientAuthentication, const SafeString& caCertificates) {
        return std::make_unique<HttpsServer>(address, port, std::move(requestHandlers), serviceContext,
                                             enforceClientAuthentication, caCertificates);
    };
    factories.enrolmentServerFactory = factories.teeServerFactory;
    factories.adminServerFactory = factories.teeServerFactory;
    return factories;
}

PcServiceContext
StaticData::makePcServiceContext(std::optional<decltype(Factories::databaseFactory)> customDatabaseFactory)
{
    auto factories = makeMockFactories();
    if (customDatabaseFactory)
    {
        factories.databaseFactory = *customDatabaseFactory;
    }
    return PcServiceContext(Configuration::instance(), std::move(factories));
}
