#include "test/util/StaticData.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/database/RedisClient.hxx"
#include "mock/client/TlsCertificateVerifierNoVerificationImplementation.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/tpm/TpmMock.hxx"
#include "mock/tsl/MockTslManager.hxx"
#include "mock/tsl/UrlRequestSenderMock.hxx"
#include "src/shared/hsm/production/ProductionVsdmKeyBlobDatabase.hxx"
#include "test/erp/pc/popp/PoPPCertificateVerifierServiceMock.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
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


Database::Factory StaticData::makeDatabaseFactory(ConnectionFactory connFactory)
{
    return [md = std::shared_ptr<MockDatabase>{}, connFactory](HsmPool& hsmPool, KeyDerivation& keyDerivation) mutable {
        std::unique_ptr<ErpDatabaseBackend> backend;
        if (TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
        {
            backend = std::make_unique<PostgresBackend>(connFactory());
        }
        else
        {
            if (!md)
            {
                md = std::make_shared<MockDatabase>(hsmPool);
            }
            backend = std::make_unique<MockDatabaseProxy>(md);
        }
        return std::make_unique<DatabaseFrontend>(std::move(backend), hsmPool, keyDerivation);
    };
}

Factories StaticData::makeMockFactories()
{
    Factories factories;
    fillMockBaseFactories(factories);
    factories.databaseFactory = makeDatabaseFactory(&PostgresBackend::mainConnection);
    if (PostgresBackend::haveReadOnlyConnection())
    {
        if (TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
        {
            factories.readOnlyDatabaseFactory = makeDatabaseFactory(&PostgresBackend::readOnlyConnection);
        }
        else
        {
            factories.readOnlyDatabaseFactory = factories.databaseFactory;
        }
    }
    factories.redisClientFactory = [](std::chrono::milliseconds socketTimeout) {
        return TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_REDIS_MOCK, true)
                   ? std::unique_ptr<RedisInterface>(new MockRedisStore())
                   : std::unique_ptr<RedisInterface>(new RedisClient(socketTimeout));
    };

    factories.jsonValidatorFactory = StaticData::getJsonValidator;

    factories.teeServerFactory = &StaticData::nullHttpsServer;

    factories.poppServiceFactory = [](boost::asio::io_context*, TslManager&, std::shared_ptr<CrlProvider>) {
        auto poppServiceMock = std::make_unique<PoPPCertificateVerifierServiceMock>();
        setupDefaultMock(*poppServiceMock);
        return poppServiceMock;
    };

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
    factories.poppServiceFactory = makePoPPServiceFactoryWithSenderMock();
    return factories;
}

PcServiceContext
StaticData::makePcServiceContext(Database::Factory customDatabaseFactory)
{
    auto factories = makeMockFactories();
    if (customDatabaseFactory)
    {
        if (PostgresBackend::haveReadOnlyConnection())
        {
            factories.readOnlyDatabaseFactory = customDatabaseFactory;
        }
        factories.databaseFactory = std::move(customDatabaseFactory);
    }
    return PcServiceContext(Configuration::instance(), std::move(factories));
}

Factories::PoPPServiceFactoryT StaticData::makePoPPServiceFactoryWithSenderMock()
{
    return [](gsl::not_null<boost::asio::io_context*> ioContext, TslManager& tslManager, std::shared_ptr<CrlProvider>) mutable {
        const auto entityStatementUrl =
            Configuration::instance().getStringValue(ConfigurationKey::POPP_ENTITY_STATEMENT_URL);
        ModelExpect(!entityStatementUrl.empty(), "POPP_ENTITY_STATEMENT_URL is empty");
        const std::string entityStatementUri{"https://erp-mock-idp:8443/popp/jwks.json"};

        auto senderMock = std::make_unique<UrlRequestSenderMock>(
            TlsCertificateVerifierNoVerificationImplementation::withVerificationDisabledForTesting(),
            std::chrono::seconds{
                Configuration::instance().getIntValue(ConfigurationKey::HTTPCLIENT_CONNECT_TIMEOUT_SECONDS)},
            std::chrono::milliseconds{
                Configuration::instance().getIntValue(ConfigurationKey::HTTPCLIENT_RESOLVE_TIMEOUT_MILLISECONDS)});

        const std::string entityStatement{
        "eyJraWQiOiJsVVpqZHhWRHpzM1hxdjBjQ2dTd1Ntc3lETnFUWThicE5CVFUxZ2Vjcl9ZIiwidHlwIjoiSldUIiwiYWxnIjoiRVMyNTYifQ."
        "eyJpc3MiOiJodHRwczovL2VycC1tb2NrLWlkcDo4NDQzIiwic3ViIjoiaHR0cHM6Ly9lcnAtbW9jay1pZHA6ODQ0MyIsImlhdCI6MTc3MjU0MT"
        "ExNiwiZXhwIjoxNzcyNjI3NTE2LCJhdXRob3JpdHlfaGludHMiOlsiaHR0cHM6Ly9hcHAtcmVmLmZlZGVyYXRpb25tYXN0ZXIuZGUiXSwibWV0"
        "YWRhdGEiOnsiZmVkZXJhdGlvbl9lbnRpdHkiOnsib3JnYW5pemF0aW9uX25hbWUiOiJlcnAtbW9jay1pZHAgUG9QUCBTZXJ2ZXIifSwib2F1dG"
        "hfcmVzb3VyY2UiOnsic2lnbmVkX2p3a3NfdXJpIjoiaHR0cHM6Ly9lcnAtbW9jay1pZHA6ODQ0My9wb3BwL2p3a3MuanNvbiJ9fSwiandrcyI6"
        "eyJrZXlzIjpbeyJrdHkiOiJFQyIsInVzZSI6InNpZyIsImNydiI6IlAtMjU2Iiwia2lkIjoibFVaamR4VkR6czNYcXYwY0NnU3dTbXN5RE5xVF"
        "k4YnBOQlRVMWdlY3JfWSIsIngiOiIyQXdkaktXVS1WazZxdzZSOHM2Q1psQ3RLVDZxc0ZIYUVnNC1aMEMxbmVnIiwieSI6Im0xN3VNcmhJdlJ3"
        "ZEVhUnZwME9hajBWOXl0dWotbkNPeW9fUjhpSlJfOXciLCJhbGciOiJFUzI1NiJ9XX19.SxCvy-"
        "powK8oTUJxq2X4yQr8XMpHgvHataDHdTmbZo5PA1EjdB0DwM71IZW7Giqmr8tA3-vPAjurTeh1RB2mCg"};
        const std::string jwks{
        "eyJraWQiOiJsVVpqZHhWRHpzM1hxdjBjQ2dTd1Ntc3lETnFUWThicE5CVFUxZ2Vjcl9ZIiwidHlwIjoiSldUIiwiYWxnIjoiRVMyNTYifQ."
        "eyJqd2tzIjp7ImtleXMiOlt7Imt0eSI6IkVDIiwieDV0I1MyNTYiOiJUbHluTzdxT1MtbTg5WndGSnZDVGxMZWtnd3VBQjZON0sxRXhnc3JVX3"
        "E4IiwiY3J2IjoiUC0yNTYiLCJraWQiOiI2MDE4NTcxMzQ0MDQzMDU4NzgwMjcwNTUyNjkzNTczMDMzMzQ4MzU4ODkwOTIzMjkiLCJ4NWMiOlsi"
        "TUlJQmpUQ0NBVE9nQXdJQkFnSVVhV3cxRWJ4aW1HdGp6eCsyaG1rNDdBQ1wvdHVrd0NnWUlLb1pJemowRUF3SXdIREVhTUJnR0ExVUVBd3dSYl"
        "c5amF5MXdiM0J3TFhOcFoyNXBibWN3SGhjTk1qWXdNakkxTVRJek5qUXhXaGNOTXpZd01qSXpNVEl6TmpReFdqQWNNUm93R0FZRFZRUUREQkZ0"
        "YjJOckxYQnZjSEF0YzJsbmJtbHVaekJaTUJNR0J5cUdTTTQ5QWdFR0NDcUdTTTQ5QXdFSEEwSUFCTmdNSFl5bGxQbFpPcXNPa2ZMT2dtWlFyU2"
        "srcXJCUjJoSU9QbWRBdFozb20xN3VNcmhJdlJ3ZEVhUnZwME9hajBWOXl0dWorbkNPeW9cL1I4aUpSXC85eWpVekJSTUIwR0ExVWREZ1FXQkJT"
        "cFZQSTdyamwwVTlpd0pRYVVISm9cL3h5MU04ekFmQmdOVkhTTUVHREFXZ0JTcFZQSTdyamwwVTlpd0pRYVVISm9cL3h5MU04ekFQQmdOVkhSTU"
        "JBZjhFQlRBREFRSFwvTUFvR0NDcUdTTTQ5QkFNQ0EwZ0FNRVVDSVFENUQ3ZHBCMW4waVRoZjZ0U2UxeGUxQ2ozeGFhQm10YWs5aVV6cFRjN25T"
        "Z0lnY0plRDhHb3dBOWE4WGJXZE1oc3p0M1VlZnNVNzVsSG5YNDhlVFNXd1Aybz0iXSwieCI6IjJBd2RqS1dVLVZrNnF3NlI4czZDWmxDdEtUNn"
        "FzRkhhRWc0LVowQzFuZWciLCJ5IjoibTE3dU1yaEl2UndkRWFSdnAwT2FqMFY5eXR1ai1uQ095b19SOGlKUl85dyJ9LHsia3R5IjoiRUMiLCJ4"
        "NXQjUzI1NiI6Ikl4T0toSzgtbzZoY3BXd2FDSjlUOEMwY01zOGc3Uk5TSFhFd2t1ZU5tTGciLCJ1c2UiOiJzaWciLCJjcnYiOiJQLTI1NiIsIm"
        "tpZCI6IjIzOTM3MjU2MTg3ODIxOTMwMzMxMjEwNTcyNTU1MTYxODQ4ODg5IiwieDVjIjpbIk1JSURRakNDQXVpZ0F3SUJBZ0lRRWdJbDJWdDVT"
        "K0dURmV5WWltcE1PVEFLQmdncWhrak9QUVFEQWpDQmhqRUxNQWtHQTFVRUJoTUNSRVV4SHpBZEJnTlZCQW9NRm1GamFHVnNiM01nUjIxaVNDQk"
        "9UMVF0VmtGTVNVUXhNakF3QmdOVkJBc01LVXR2YlhCdmJtVnVkR1Z1TFVOQklHUmxjaUJVWld4bGJXRjBhV3RwYm1aeVlYTjBjblZyZEhWeU1T"
        "SXdJQVlEVlFRRERCbEJRMHhQVXk1TFQwMVFMVU5CTWpBZ1ZFVlRWQzFQVGt4Wk1CNFhEVEkyTURJeU5USXpNREF3TUZvWERUSTRNREl5TkRJek"
        "1EQXdNRm93WURFTE1Ba0dBMVVFQmhNQ1JFVXhLekFwQmdOVkJBb01JbUZqYUdWc2IzTWdSMjFpU0NCVVJWTlVMVTlPVEZrZ0xTQk9UMVF0VmtG"
        "TVNVUXhKREFpQmdOVkJBTU1HM0J2Y0hBdGRHVnpkQzB5TG5ScExXUnBaVzV6ZEdVdWRHVnpkREJaTUJNR0J5cUdTTTQ5QWdFR0NDcUdTTTQ5QX"
        "dFSEEwSUFCUEVDMUExUjJzSitxblBJSzlTeGcxY3M4V1JwelVCWmhzTDl5NWVtZHczWVptWnBrakc1a2ZodGQxYlNXUVNJZldVUzJVU3o1K2Yw"
        "cTBFc0ZmREpnVmlqZ2dGYk1JSUJWekFkQmdOVkhRNEVGZ1FVUk5lVGhVNDBQQVl4MnFyeGVxbjEwRDZTYVlNd0RnWURWUjBQQVFIXC9CQVFEQW"
        "daQU1DUUdBMVVkRVFRZE1CdUNHWEJ2Y0hBdGRHVnpkQzUwYVMxa2FXVnVjM1JsTG5SbGMzUXdEQVlEVlIwVEFRSFwvQkFJd0FEQWhCZ05WSFNB"
        "RUdqQVlNQW9HQ0NxQ0ZBQk1CSUVqTUFvR0NDcUNGQUJNQklJZk1FWUdDQ3NHQVFVRkJ3RUJCRG93T0RBMkJnZ3JCZ0VGQlFjd0FZWXFhSFIwY0"
        "RvdkwyOWpjM0F0ZEdWemRDNXZZM053TG5SbGJHVnRZWFJwYXkxMFpYTjBPamd3T0RBdk1COEdBMVVkSXdRWU1CYUFGR3pud2ZiaFFFeEpBNkpa"
        "dlpHeDFMSlNlVXk1TUZzR0JTc2tDQU1EQkZJd1VEQk9NRXd3U2pCSU1Eb01PRlJ2YTJWdUxWTnBaMjVoZEhWeUxVbGtaVzUwYVhURHBIUWdac0"
        "84Y2lCUWNtOXZaaUJ2WmlCUVlYUnBaVzUwSUZCeVpYTmxibU5sTUFvR0NDcUNGQUJNQklKQU1Ba0dBMVVkSlFRQ01BQXdDZ1lJS29aSXpqMEVB"
        "d0lEU0FBd1JRSWdHTTRCNk4wY1wvTzR0KzF3dnhcL0VqbXFMNlBGOWY2cGhvVHJ1NTZIM3FPZTRDSVFDV2Ztalp2UHFhYTdZT1A1dzZ0bmI2T0"
        "d4VUJMS3paUFNONDhjZFhkYnNBUT09Il0sIngiOiI4UUxVRFZIYXduNnFjOGdyMUxHRFZ5enhaR25OUUZtR3d2M0xsNlozRGRnIiwieSI6Ilpt"
        "WnBrakc1a2ZodGQxYlNXUVNJZldVUzJVU3o1LWYwcTBFc0ZmREpnVmcifSx7Imt0eSI6IkVDIiwieDV0I1MyNTYiOiIydkljMnk1c3c1T2czdl"
        "poWVJQcGE4SEhIVk5LRkg3ZUxPajJzX0pNX01RIiwidXNlIjoic2lnIiwiY3J2IjoiUC0yNTYiLCJraWQiOiIxODY3NzA0MDkyNTcxNDIxOTkz"
        "ODc3MDEzNDg3MjY2Mjc3NTg4MCIsIng1YyI6WyJNSUlEUWpDQ0F1aWdBd0lCQWdJUURnMFJOcFdLUVdpZnFEQWdNM3lZU0RBS0JnZ3Foa2pPUF"
        "FRREFqQ0JoakVMTUFrR0ExVUVCaE1DUkVVeEh6QWRCZ05WQkFvTUZtRmphR1ZzYjNNZ1IyMWlTQ0JPVDFRdFZrRk1TVVF4TWpBd0JnTlZCQXNN"
        "S1V0dmJYQnZibVZ1ZEdWdUxVTkJJR1JsY2lCVVpXeGxiV0YwYVd0cGJtWnlZWE4wY25WcmRIVnlNU0l3SUFZRFZRUUREQmxCUTB4UFV5NUxUMD"
        "FRTFVOQk1qQWdWRVZUVkMxUFRreFpNQjRYRFRJMk1ESXlOVEl6TURBd01Gb1hEVEk0TURJeU5ESXpNREF3TUZvd1lERUxNQWtHQTFVRUJoTUNS"
        "RVV4S3pBcEJnTlZCQW9NSW1GamFHVnNiM01nUjIxaVNDQlVSVk5VTFU5T1RGa2dMU0JPVDFRdFZrRk1TVVF4SkRBaUJnTlZCQU1NRzNCdmNIQX"
        "RkR1Z6ZEMwekxuUnBMV1JwWlc1emRHVXVkR1Z6ZERCWk1CTUdCeXFHU000OUFnRUdDQ3FHU000OUF3RUhBMElBQkRqeXNhSW1XNFgwemxMTmtr"
        "WTd1cjJCWENuZ0hcLzJQdXlcL2V1ZXhRN296bXFSaEs1U1NaOXlhWlNROE1jXC9nWWdnbmthcjdcL2RaTStNaG5uZmRVNlNVYWpnZ0ZiTUlJQl"
        "Z6QWRCZ05WSFE0RUZnUVUxYjZBdjBDQmJEZjd2Ulp4cTU4czJZcjA0R1F3RGdZRFZSMFBBUUhcL0JBUURBZ1pBTUNRR0ExVWRFUVFkTUJ1Q0dY"
        "QnZjSEF0ZEdWemRDNTBhUzFrYVdWdWMzUmxMblJsYzNRd0RBWURWUjBUQVFIXC9CQUl3QURBaEJnTlZIU0FFR2pBWU1Bb0dDQ3FDRkFCTUJJRW"
        "pNQW9HQ0NxQ0ZBQk1CSUlmTUVZR0NDc0dBUVVGQndFQkJEb3dPREEyQmdnckJnRUZCUWN3QVlZcWFIUjBjRG92TDI5amMzQXRkR1Z6ZEM1dlkz"
        "TndMblJsYkdWdFlYUnBheTEwWlhOME9qZ3dPREF2TUI4R0ExVWRJd1FZTUJhQUZHem53ZmJoUUV4SkE2Slp2Wkd4MUxKU2VVeTVNRnNHQlNza0"
        "NBTURCRkl3VURCT01Fd3dTakJJTURvTU9GUnZhMlZ1TFZOcFoyNWhkSFZ5TFVsa1pXNTBhWFREcEhRZ1pzTzhjaUJRY205dlppQnZaaUJRWVhS"
        "cFpXNTBJRkJ5WlhObGJtTmxNQW9HQ0NxQ0ZBQk1CSUpBTUFrR0ExVWRKUVFDTUFBd0NnWUlLb1pJemowRUF3SURTQUF3UlFJaEFJVkRES0RnUm"
        "1zNnpWWVBZaDNZQUlVQTRSV1NnZUtnVmRnaGI2Nkx6OWl6QWlCMXBlQStwRmM1QmJKcGRzVXJnR1RnRzZha0tIT0VXd0FzRUtleUlMMElFZz09"
        "Il0sIngiOiJPUEt4b2laYmhmVE9VczJTUmp1NnZZRmNLZUFmX1ktN0w5NjU3RkR1ak9ZIiwieSI6InFSaEs1U1NaOXlhWlNROE1jX2dZZ2dua2"
        "FyN19kWk0tTWhubmZkVTZTVVkifSx7Imt0eSI6IkVDIiwieDV0I1MyNTYiOiItSnpQd2lYWVNoZFp0OTRLcUpIUkFvTGVHOFVkVDhiTmhtX1Bz"
        "WEpuQnNzIiwidXNlIjoic2lnIiwiY3J2IjoiUC0yNTYiLCJraWQiOiIyMTgwNTg4Nzk4Nzg4MTI1NzE2ODAzODgxNjMwOTEyMTY5MjE3MDIiLC"
        "J4NWMiOlsiTUlJRFFUQ0NBdWVnQXdJQkFnSVJBS1FNbk5YMmYwT2lvZG03cHF0bTlHWXdDZ1lJS29aSXpqMEVBd0l3Z1lZeEN6QUpCZ05WQkFZ"
        "VEFrUkZNUjh3SFFZRFZRUUtEQlpoWTJobGJHOXpJRWR0WWtnZ1RrOVVMVlpCVEVsRU1USXdNQVlEVlFRTERDbExiMjF3YjI1bGJuUmxiaTFEUV"
        "NCa1pYSWdWR1ZzWlcxaGRHbHJhVzVtY21GemRISjFhM1IxY2pFaU1DQUdBMVVFQXd3WlFVTk1UMU11UzA5TlVDMURRVEl3SUZSRlUxUXRUMDVN"
        "V1RBZUZ3MHlOakF5TWpVeU16QXdNREJhRncweU5URXlNekV5TXpBd01EQmFNRjR4Q3pBSkJnTlZCQVlUQWtSRk1Tc3dLUVlEVlFRS0RDSmhZMm"
        "hsYkc5eklFZHRZa2dnVkVWVFZDMVBUa3haSUMwZ1RrOVVMVlpCVEVsRU1TSXdJQVlEVlFRRERCbHdiM0J3TFhSbGMzUXVkR2t0WkdsbGJuTjBa"
        "UzUwWlhOME1Ga3dFd1lIS29aSXpqMENBUVlJS29aSXpqMERBUWNEUWdBRUhIaTlzUnBKckhET3FVbzhpcVNjVmVlbFwvblhTNUFza1pHOEwzeF"
        "MzWkxPR2d3cnNJeDlkQTBrcCtjRjFPZ3JTclFEUDg0cTJXaFwvMnFzNG1JdG1KcmFPQ0FWc3dnZ0ZYTUIwR0ExVWREZ1FXQkJROTNBcXhvcHFU"
        "dVIwUGFKd0ZzQW9DMUdcL1I5VEFPQmdOVkhROEJBZjhFQkFNQ0JrQXdKQVlEVlIwUkJCMHdHNElaY0c5d2NDMTBaWE4wTG5ScExXUnBaVzV6ZE"
        "dVdWRHVnpkREFNQmdOVkhSTUJBZjhFQWpBQU1DRUdBMVVkSUFRYU1CZ3dDZ1lJS29JVUFFd0VnU013Q2dZSUtvSVVBRXdFZ2g4d1JnWUlLd1lC"
        "QlFVSEFRRUVPakE0TURZR0NDc0dBUVVGQnpBQmhpcG9kSFJ3T2k4dmIyTnpjQzEwWlhOMExtOWpjM0F1ZEdWc1pXMWhkR2xyTFhSbGMzUTZPRE"
        "E0TUM4d0h3WURWUjBqQkJnd0ZvQVViT2ZCOXVGQVRFa0RvbG05a2JIVXNsSjVUTGt3V3dZRkt5UUlBd01FVWpCUU1FNHdUREJLTUVnd09ndzRW"
        "RzlyWlc0dFUybG5ibUYwZFhJdFNXUmxiblJwZE1Pa2RDQm13N3h5SUZCeWIyOW1JRzltSUZCaGRHbGxiblFnVUhKbGMyVnVZMlV3Q2dZSUtvSV"
        "VBRXdFZ2tBd0NRWURWUjBsQkFJd0FEQUtCZ2dxaGtqT1BRUURBZ05JQURCRkFpQWZ6TjlcL3JJRXZ6Q3VwM29jclcxUDNvVjdMc0lmUGF6RWRF"
        "cGV2aGJ4enJ3SWhBTCt5SVp2NjhlcDF5emhteFlXdHdWcmRKQ3VkUVVhQUxEME9SWERXXC9YbFYiXSwieCI6IkhIaTlzUnBKckhET3FVbzhpcV"
        "NjVmVlbF9uWFM1QXNrWkc4TDN4UzNaTE0iLCJ5IjoiaG9NSzdDTWZYUU5KS2ZuQmRUb0swcTBBel9PS3Rsb2Y5cXJPSmlMWmlhMCJ9XX19."
        "YjX94z3k8gdHUgF7L-HlXSx-81Yu7ANA5Or2EW22zaQhm_7guLOzxs5vQDQqFdZDrhFVzDhnJYDbXROrFiqY8g"};

        senderMock->setUrlHandler(entityStatementUrl, [entityStatement](const std::string&) -> ClientResponse {
            return ClientResponse{Header{HttpStatus::OK}, entityStatement};
        });
        senderMock->setUrlHandler(entityStatementUri, [jwks](const std::string&) -> ClientResponse {
            return ClientResponse{Header{HttpStatus::OK}, jwks};
        });

        return std::make_unique<PoPPCertificateVerifierService>(*ioContext, std::move(senderMock), tslManager);
    };
}
