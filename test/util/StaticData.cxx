#include "test/util/StaticData.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/database/RedisClient.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/tpm/TpmMock.hxx"
#include "mock/tsl/MockTslManager.hxx"
#include "src/shared/hsm/production/ProductionVsdmKeyBlobDatabase.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/mock/MockVsdmKeyBlobDatabase.hxx"
#include "test/mock/RegistrationMock.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/TestConfiguration.hxx"


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
    fillMockBaseFactories(factories);
    factories.databaseFactory = [](HsmPool& hsmPool, KeyDerivation& keyDerivation) -> std::unique_ptr<Database> {
        std::unique_ptr<ErpDatabaseBackend> backend;
        if (TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
            backend = std::make_unique<PostgresBackend>();
        else
            backend = std::make_unique<MockDatabase>(hsmPool);
        return std::make_unique<DatabaseFrontend>(std::move(backend), hsmPool, keyDerivation);
    };
    factories.redisClientFactory = [](std::chrono::milliseconds socketTimeout) {
        return TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_REDIS_MOCK, true)
                   ? std::unique_ptr<RedisInterface>(new MockRedisStore())
                   : std::unique_ptr<RedisInterface>(new RedisClient(socketTimeout));
    };

    factories.jsonValidatorFactory = StaticData::getJsonValidator;

    factories.teeServerFactory = &StaticData::nullHttpsServer;

    return factories;
}

Factories StaticData::makeMockFactoriesWithServers()
{
    auto factories = makeMockFactories();
    factories.teeServerFactory = [](const std::string_view address, uint16_t port,
                                    RequestHandlerManager&& requestHandlers, BaseServiceContext& serviceContext,
                                    bool enforceClientAuthentication, const SafeString& caCertificates) {
        return std::make_unique<HttpsServer>(address, port, std::move(requestHandlers), dynamic_cast<PcServiceContext&>(serviceContext),
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
