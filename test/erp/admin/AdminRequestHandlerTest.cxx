/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/admin/AdminRequestHandler.hxx"
#include "erp/admin/AdminServer.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/util/ConfigurationFormatter.hxx"
#include "erp/util/RuntimeConfiguration.hxx"
#include "mock/util/MockConfiguration.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/server/AccessLog.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "test/mock/MockTerminationHandler.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>

class AdminRequestHandlerTest : public testing::Test
{
public:
    AdminRequestHandlerTest()
    {
        MockTerminationHandler::setupForTesting();
        header.addHeaderField(Header::ContentType, ContentMimeType::xWwwFormUrlEncoded);
        header.addHeaderField(Header::Authorization, "Basic cred");
    }

    ~AdminRequestHandlerTest() override
    {
        // reset state to running for other tests.
        MockTerminationHandler::setupForTesting();
    }

    boost::asio::io_context mIoContext;
    EnvironmentVariableGuard adminApiAuth{"ERP_ADMIN_CREDENTIALS", "cred"};
    EnvironmentVariableGuard defaultTimeout{"ERP_ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS", "2"};
    PcServiceContext serviceContext{StaticData::makePcServiceContext()};
    Header header;
    ServerResponse response;
    AccessLog accessLog;
};


TEST_F(AdminRequestHandlerTest, SuccessfulShutdownDefaultDelay)
{
    ServerRequest request{Header(header)};
    SessionContext session{serviceContext, request, response, accessLog};
    PostRestartHandler restartHandler{ConfigurationKey::ADMIN_CREDENTIALS,
                                      ConfigurationKey::ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS};
    EXPECT_NO_THROW(restartHandler.handleRequest(session));
    EXPECT_EQ(session.response.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(session.response.getBody(), "shutdown in 2 seconds");
}

TEST_F(AdminRequestHandlerTest, SuccessfulShutdownParameterDelay)
{
    ServerRequest request{Header(header)};
    request.setBody("delay-seconds=3");
    SessionContext session{serviceContext, request, response, accessLog};
    PostRestartHandler restartHandler{ConfigurationKey::ADMIN_CREDENTIALS,
                                      ConfigurationKey::ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS};
    EXPECT_NO_THROW(restartHandler.handleRequest(session));
    EXPECT_EQ(session.response.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(session.response.getBody(), "shutdown in 3 seconds");
}

TEST_F(AdminRequestHandlerTest, SuccessfulShutdownParameterImmediate)
{
    ServerRequest request{Header(header)};
    request.setBody("delay-seconds=0");
    SessionContext session{serviceContext, request, response, accessLog};
    PostRestartHandler restartHandler{ConfigurationKey::ADMIN_CREDENTIALS,
                                      ConfigurationKey::ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS};
    EXPECT_NO_THROW(restartHandler.handleRequest(session));
    EXPECT_EQ(session.response.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(session.response.getBody(), "shutdown in 0 seconds");
}

TEST_F(AdminRequestHandlerTest, FailedShutdownWrongParameter)
{
    ServerRequest request{Header(header)};
    request.setBody("delay-second=3");
    SessionContext session{serviceContext, request, response, accessLog};
    PostRestartHandler restartHandler{ConfigurationKey::ADMIN_CREDENTIALS,
                                      ConfigurationKey::ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS};
    EXPECT_NO_THROW(restartHandler.handleRequest(session));
    EXPECT_EQ(session.response.getHeader().status(), HttpStatus::BadRequest);
}
TEST_F(AdminRequestHandlerTest, FailedShutdownWrongParameter2)
{
    ServerRequest request{Header(header)};
    request.setBody("delay-seconds=3;delay-second=3");
    SessionContext session{serviceContext, request, response, accessLog};
    PostRestartHandler restartHandler{ConfigurationKey::ADMIN_CREDENTIALS,
                                      ConfigurationKey::ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS};
    EXPECT_NO_THROW(restartHandler.handleRequest(session));
    EXPECT_EQ(session.response.getHeader().status(), HttpStatus::BadRequest);
}

TEST_F(AdminRequestHandlerTest, FailedShutdownParameterRange)
{
    ServerRequest request{Header(header)};
    request.setBody("delay-seconds=-1");
    SessionContext session{serviceContext, request, response, accessLog};
    PostRestartHandler restartHandler{ConfigurationKey::ADMIN_CREDENTIALS,
                                      ConfigurationKey::ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS};
    EXPECT_NO_THROW(restartHandler.handleRequest(session));
    EXPECT_EQ(session.response.getHeader().status(), HttpStatus::BadRequest);
}

TEST_F(AdminRequestHandlerTest, ConfigurationHandler)
{
    ServerRequest request{Header(header)};
    SessionContext session{serviceContext, request, response, accessLog};
    GetConfigurationHandler handler{
        ConfigurationKey::ADMIN_CREDENTIALS,
        std::make_unique<erp::ConfigurationFormatter>(std::make_shared<const erp::RuntimeConfiguration>())};
    EXPECT_NO_THROW(handler.handleRequest(session));
    EXPECT_EQ(session.response.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(session.response.getHeader().header(Header::ContentType), MimeType::json);

    rapidjson::Document configDocument;
    configDocument.Parse(session.response.getBody());
    std::cout << session.response.getBody() << std::endl;

    OpsConfigKeyNames confNames;

    // credential should be redacted
    {
        const auto credKey = confNames.strings(ConfigurationKey::ADMIN_CREDENTIALS);
        rapidjson::Pointer credKeyPointer{
            std::string{"/environment/"}.append(credKey.environmentVariable).append("/value")};
        ASSERT_NE(credKeyPointer, nullptr);
        EXPECT_EQ(std::string(credKeyPointer.Get(configDocument)->GetString()), "<redacted>");
    }

    // credential should be redacted
    {
        const auto credKey = confNames.strings(ConfigurationKey::ADMIN_RC_CREDENTIALS);
        rapidjson::Pointer credKeyPointer{
            std::string{"/environment/"}.append(credKey.environmentVariable).append("/value")};
        ASSERT_NE(credKeyPointer, nullptr);
        EXPECT_EQ(std::string(credKeyPointer.Get(configDocument)->GetString()), "<redacted>");
    }
}
