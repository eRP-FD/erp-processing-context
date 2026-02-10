// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "exporter/MedicationExporterMain.hxx"
#include "exporter/admin/PutRuntimeConfigHandler.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "exporter/server/SessionContext.hxx"
#include "exporter/util/ConfigurationFormatter.hxx"
#include "exporter/util/RuntimeConfiguration.hxx"
#include "mock/hsm/HsmMockClient.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/tsl/MockTslManager.hxx"
#include "shared/admin/AdminRequestHandler.hxx"
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
#include <magic_enum/magic_enum_iostream.hpp>

class PutRuntimeConfigHandlerTest : public ::testing::Test
{
public:
    explicit PutRuntimeConfigHandlerTest()
    {
        header.addHeaderField(Header::ContentType, ContentMimeType::xWwwFormUrlEncoded);
        header.addHeaderField(Header::Authorization, "Basic cred");

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
    EnvironmentVariableGuard adminApiRcAuth{"ERP_MEDICATION_EXPORTER_ADMIN_RC_CREDENTIALS", "cred"};
    EnvironmentVariableGuard epaFQDNs{"ERP_MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_EPA_AS_FQDN", "localhost:8888+1"};
    std::unique_ptr<MedicationExporterServiceContext> serviceContext;
    Header header;
    ServerResponse response;
    AccessLog accessLog;

    void changeConfig(const std::string& key, const std::string& value, HttpStatus expectedStatus = HttpStatus::OK)
    {
        ServerRequest request{Header(header)};
        request.header().setMethod(HttpMethod::PUT);
        std::string body = key;
        if (! value.empty())
        {
            body.append("=" + value);
        }
        request.setBody(std::move(body));
        exporter::SessionContext session{*serviceContext, request, response, accessLog};
        exporter::PutRuntimeConfigHandler handler{ConfigurationKey::MEDICATION_EXPORTER_ADMIN_RC_CREDENTIALS};
        EXPECT_NO_THROW(handler.handleRequest(session));
        EXPECT_EQ(session.response.getHeader().status(), expectedStatus) << session.response.getBody();
    }

    struct CheckGetConfigExpectation
    {
        bool pause;
        bool epaPause;
        bool tRezeptPause;
        int64_t throttle;
    };

    void checkGetConfig(CheckGetConfigExpectation expectation)
    {
        ServerRequest request{Header(header)};
        request.header().setMethod(HttpMethod::GET);
        exporter::SessionContext session{*serviceContext, request, response, accessLog};
        GetConfigurationHandler handler{
            ConfigurationKey::MEDICATION_EXPORTER_ADMIN_CREDENTIALS,
            std::make_unique<exporter::ConfigurationFormatter>(serviceContext->getRuntimeConfiguration())};
        EXPECT_NO_THROW(handler.handleRequest(session));
        EXPECT_EQ(session.response.getHeader().status(), HttpStatus::OK);
        EXPECT_EQ(session.response.getHeader().header(Header::ContentType), MimeType::json);

        rapidjson::Document configDocument;
        configDocument.Parse(session.response.getBody());

        rapidjson::Pointer pausePointer{std::string{"/runtime/Pause/value"}};
        rapidjson::Pointer epaPausePointer{std::string{"/runtime/Pause/epa/value"}};
        rapidjson::Pointer tRezeptPausePointer{std::string{"/runtime/Pause/t-rezept/value"}};
        rapidjson::Pointer throttlePointer{std::string{"/runtime/Throttle/value"}};
        EXPECT_EQ(pausePointer.Get(configDocument)->GetBool(), expectation.pause);
        EXPECT_EQ(epaPausePointer.Get(configDocument)->GetBool(), expectation.epaPause);
        EXPECT_EQ(tRezeptPausePointer.Get(configDocument)->GetBool(), expectation.tRezeptPause);
        EXPECT_EQ(throttlePointer.Get(configDocument)->GetInt64(), expectation.throttle);
    }
};

TEST_F(PutRuntimeConfigHandlerTest, PauseResumeThrottle)
{
    CheckGetConfigExpectation expect{.pause = false, .epaPause = false, .tRezeptPause = false, .throttle = 0};
    checkGetConfig(expect);

    changeConfig("Pause", "");
    expect = {.pause = true, .epaPause = true, .tRezeptPause = true, .throttle = 0};
    checkGetConfig(expect);

    changeConfig("Pause", "");
    checkGetConfig(expect);

    changeConfig("Throttle", "1500");
    expect.throttle = 1500;
    checkGetConfig(expect);

    changeConfig("Throttle", "1500");
    checkGetConfig(expect);

    changeConfig("Resume", "");
    expect = {.pause = false, .epaPause = false, .tRezeptPause = false, .throttle = 1500};
    checkGetConfig(expect);

    changeConfig("Resume", "");
    checkGetConfig(expect);

    changeConfig("Throttle", "0");
    expect.throttle = 0;
    checkGetConfig(expect);

    changeConfig("Throttle", "0");
    checkGetConfig(expect);

    changeConfig("Pause", "epa");
    expect.epaPause = true;
    checkGetConfig(expect);

    changeConfig("Pause", "t-rezept");
    expect.tRezeptPause = true;
    expect.pause = true;
    checkGetConfig(expect);

    changeConfig("Resume", "epa");
    expect.epaPause = false;
    expect.pause = false;
    checkGetConfig(expect);

    changeConfig("Resume", "t-rezept");
    expect.tRezeptPause = false;
    checkGetConfig(expect);

    changeConfig("Pause", "t-rezept");
    expect.tRezeptPause = true;
    checkGetConfig(expect);

    changeConfig("Resume", "epa");
    checkGetConfig(expect);

    changeConfig("Resume", "");
    expect.tRezeptPause = false;
    checkGetConfig(expect);

    changeConfig("Pause", "true");
    expect = {.pause = true, .epaPause = true, .tRezeptPause = true, .throttle = 0};
    checkGetConfig(expect);

    changeConfig("Pause", "false", HttpStatus::BadRequest);
    checkGetConfig(expect);

    changeConfig("Resume", "true");
    expect = {.pause = false, .epaPause = false, .tRezeptPause = false, .throttle = 0};
    checkGetConfig(expect);

    changeConfig("Pause", "");
    expect = {.pause = true, .epaPause = true, .tRezeptPause = true, .throttle = 0};
    checkGetConfig(expect);

    changeConfig("Resume", "false", HttpStatus::BadRequest);
    checkGetConfig(expect);

    changeConfig("Resume", "true");
    expect = {.pause = false, .epaPause = false, .tRezeptPause = false, .throttle = 0};
    checkGetConfig(expect);
}

using magic_enum::ostream_operators::operator<<;

class PutRuntimeConfigHandlerTimingThresholdTest : public PutRuntimeConfigHandlerTest,
                                                   public testing::WithParamInterface<DurationCategory>
{
public:
    void changeThreshold(const std::string& newVal)
    {
        auto category = GetParam();
        auto paramName = std::string{magic_enum::enum_name(category)}.append("MetricLogThresholdMs=");
        header.setMethod(HttpMethod::PUT);
        ServerRequest request{Header(header)};
        std::string body = paramName + newVal;
        request.setBody(std::move(body));
        exporter::SessionContext session{*serviceContext, request, response, accessLog};
        exporter::PutRuntimeConfigHandler handler{ConfigurationKey::MEDICATION_EXPORTER_ADMIN_RC_CREDENTIALS};
        EXPECT_NO_THROW(handler.handleRequest(session));
        EXPECT_EQ(session.response.getHeader().status(), HttpStatus::OK);
    }
    void checkGetConfigThreshold(DurationCategory category, int64_t expectedThreshold)
    {
        header.setMethod(HttpMethod::GET);
        ServerRequest request{Header(header)};
        exporter::SessionContext session{*serviceContext, request, response, accessLog};
        GetConfigurationHandler handler{
            ConfigurationKey::MEDICATION_EXPORTER_ADMIN_CREDENTIALS,
            std::make_unique<exporter::ConfigurationFormatter>(serviceContext->getRuntimeConfiguration())};
        EXPECT_NO_THROW(handler.handleRequest(session));
        EXPECT_EQ(session.response.getHeader().status(), HttpStatus::OK);
        EXPECT_EQ(session.response.getHeader().header(Header::ContentType), MimeType::json);

        rapidjson::Document configDocument;
        configDocument.Parse(session.response.getBody());

        auto paramName = std::string{magic_enum::enum_name(category)}.append("MetricLogThresholdMs");

        rapidjson::Pointer paramPointer{std::string{"/runtime/"}.append(paramName).append("/value")};
        EXPECT_EQ(paramPointer.Get(configDocument)->GetInt64(), expectedThreshold);

        EXPECT_EQ(serviceContext->getRuntimeConfigurationGetter()->getMetricsLogThresholdMs(category).count(),
                  expectedThreshold);
    }
};

TEST_P(PutRuntimeConfigHandlerTimingThresholdTest, test)
{
    auto category = GetParam();
    auto defaultValue = shared::RuntimeConfigurationBase::defaultMetricsLogThresholdsMs().at(category).count();
    checkGetConfigThreshold(GetParam(), defaultValue);
    changeThreshold("0");
    checkGetConfigThreshold(GetParam(), 0);
    changeThreshold("1000");
    checkGetConfigThreshold(GetParam(), 1000);
    changeThreshold("");
    checkGetConfigThreshold(GetParam(), defaultValue);
}

INSTANTIATE_TEST_SUITE_P(x, PutRuntimeConfigHandlerTimingThresholdTest,
                         testing::ValuesIn(magic_enum::enum_values<DurationCategory>()));
