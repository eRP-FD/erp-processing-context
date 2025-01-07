/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/admin/AdminRequestHandler.hxx"
#include "exporter/MedicationExporterMain.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "exporter/server/SessionContext.hxx"
#include "exporter/util/ConfigurationFormatter.hxx"
#include "exporter/util/RuntimeConfiguration.hxx"
#include "mock/hsm/HsmMockClient.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/tsl/MockTslManager.hxx"
#include "mock/util/MockConfiguration.hxx"
#include "shared/hsm/production/ProductionVsdmKeyBlobDatabase.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/server/AccessLog.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockVsdmKeyBlobDatabase.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/TestConfiguration.hxx"

#include <gtest/gtest.h>

class AdminRequestHandlerTest : public testing::Test
{
public:
    AdminRequestHandlerTest()
    {
        header.addHeaderField(Header::ContentType, ContentMimeType::xWwwFormUrlEncoded);
        header.addHeaderField(Header::Authorization, "Basic cred");
        header.setMethod(HttpMethod::GET);

        auto factories = MedicationExporterMain::createProductionFactories();
        factories.vsdmKeyBlobDatabaseFactory = []() -> std::unique_ptr<VsdmKeyBlobDatabase> {
            if (TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
                return std::make_unique<ProductionVsdmKeyBlobDatabase>();
            else
                return std::make_unique<MockVsdmKeyBlobDatabase>();
        };
        factories.blobCacheFactory = [] {
            return MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm);
        };
        factories.hsmClientFactory = []() {
            return std::make_unique<HsmMockClient>();
        };
        factories.hsmFactoryFactory = [](std::unique_ptr<HsmClient>&&, std::shared_ptr<BlobCache> blobCache) {
            MockBlobCache::setupBlobCache(MockBlobCache::MockTarget::MockedHsm, *blobCache);
            return std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(), std::move(blobCache));
        };
        factories.teeTokenUpdaterFactory = TeeTokenUpdater::createMockTeeTokenUpdaterFactory();
        factories.tslManagerFactory = MockTslManager::createMockTslManager;
        serviceContext = std::make_unique<MedicationExporterServiceContext>(mIoContext, Configuration::instance(),
                                                                            std::move(factories));
    }

    boost::asio::io_context mIoContext;
    EnvironmentVariableGuard adminApiAuth{"ERP_MEDICATION_EXPORTER_ADMIN_CREDENTIALS", "cred"};
    EnvironmentVariableGuard epaFQDNs{"ERP_MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_EPA_AS_FQDN", "localhost:8888+1"};
    std::unique_ptr<MedicationExporterServiceContext> serviceContext;
    Header header;
    ServerResponse response;
    AccessLog accessLog;
};


TEST_F(AdminRequestHandlerTest, ConfigurationHandler)
{
    ServerRequest request{Header(header)};
    SessionContext session{*serviceContext, request, response, accessLog};
    GetConfigurationHandler handler{
        ConfigurationKey::MEDICATION_EXPORTER_ADMIN_CREDENTIALS,
        std::make_unique<exporter::ConfigurationFormatter>(std::make_shared<const exporter::RuntimeConfiguration>())};
    EXPECT_NO_THROW(handler.handleRequest(session));
    EXPECT_EQ(session.response.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(session.response.getHeader().header(Header::ContentType), MimeType::json);

    rapidjson::Document configDocument;
    configDocument.Parse(session.response.getBody());
    std::cout << session.response.getBody() << std::endl;

    OpsConfigKeyNames confNames;

    // credential should be redacted
    {
        const auto credKey = confNames.strings(ConfigurationKey::MEDICATION_EXPORTER_ADMIN_CREDENTIALS);
        rapidjson::Pointer credKeyPointer{
            std::string{"/environment/"}.append(credKey.environmentVariable).append("/value")};
        ASSERT_NE(credKeyPointer, nullptr);
        EXPECT_EQ(std::string(credKeyPointer.Get(configDocument)->GetString()), "<redacted>");
    }

    // credential should be redacted
    {
        const auto credKey = confNames.strings(ConfigurationKey::MEDICATION_EXPORTER_ADMIN_RC_CREDENTIALS);
        rapidjson::Pointer credKeyPointer{
            std::string{"/environment/"}.append(credKey.environmentVariable).append("/value")};
        ASSERT_NE(credKeyPointer, nullptr);
        EXPECT_EQ(std::string(credKeyPointer.Get(configDocument)->GetString()), "<redacted>");
    }
}
