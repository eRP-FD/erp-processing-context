// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/admin/PutRuntimeConfigHandler.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/util/ConfigurationFormatter.hxx"
#include "erp/util/RuntimeConfiguration.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/server/AccessLog.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>

class PutRuntimeConfigHandlerTest : public ::testing::Test
{
public:
    explicit PutRuntimeConfigHandlerTest()
    {
        header.addHeaderField(Header::ContentType, ContentMimeType::xWwwFormUrlEncoded);
        header.addHeaderField(Header::Authorization, "Basic cred");
    }
    boost::asio::io_context mIoContext;
    EnvironmentVariableGuard adminApiAuth{"ERP_ADMIN_CREDENTIALS", "cred"};
    EnvironmentVariableGuard adminApiRcAuth{"ERP_ADMIN_RC_CREDENTIALS", "cred"};
    PcServiceContext serviceContext{StaticData::makePcServiceContext()};
    Header header;
    ServerResponse response;
    AccessLog accessLog;

    void changePN3(bool enable, const std::optional<model::Timestamp>& expiry)
    {
        header.setMethod(HttpMethod::PUT);
        ServerRequest request{Header(header)};
        std::string body = std::string{"AcceptPN3="} + (enable ? "True" : "fAlse");
        if (expiry)
        {
            body.append("&AcceptPN3Expiry=" + UrlHelper::escapeUrl(expiry->toXsDateTime()));
        }
        request.setBody(std::move(body));
        SessionContext session{serviceContext, request, response, accessLog};
        PutRuntimeConfigHandler handler{ConfigurationKey::ADMIN_RC_CREDENTIALS};
        EXPECT_NO_THROW(handler.handleRequest(session));
        EXPECT_EQ(session.response.getHeader().status(), HttpStatus::OK);
    }

    void checkGetConfigPn3(bool shouldBeEnabled, const std::optional<model::Timestamp>& expectedExpiry)
    {
        header.setMethod(HttpMethod::GET);
        ServerRequest request{Header(header)};
        SessionContext session{serviceContext, request, response, accessLog};
        GetConfigurationHandler handler{ConfigurationKey::ADMIN_CREDENTIALS,
                                        std::make_unique<erp::ConfigurationFormatter>(
                                            serviceContext.getRuntimeConfiguration())};
        EXPECT_NO_THROW(handler.handleRequest(session));
        EXPECT_EQ(session.response.getHeader().status(), HttpStatus::OK);
        EXPECT_EQ(session.response.getHeader().header(Header::ContentType), MimeType::json);

        rapidjson::Document configDocument;
        configDocument.Parse(session.response.getBody());

        rapidjson::Pointer acceptPn3KeyPointer{std::string{"/runtime/AcceptPN3/value"}};
        rapidjson::Pointer pn3ExpiryKeyPointer{std::string{"/runtime/AcceptPN3Expiry/value"}};
        rapidjson::Pointer pn3MaxActiveKeyPointer{std::string{"/runtime/AcceptPN3MaxActive/value"}};
        std::string expectedAcceptPN3 = shouldBeEnabled ? "true" : "false";
        EXPECT_EQ(acceptPn3KeyPointer.Get(configDocument)->GetString(), expectedAcceptPN3);
        if (expectedExpiry)
        {
            std::string expiryStr = pn3ExpiryKeyPointer.Get(configDocument)->GetString();
            ASSERT_FALSE(expiryStr.empty());
            auto expiry = model::Timestamp::fromXsDateTime(expiryStr);
            EXPECT_EQ(expiry, expectedExpiry);
        }
        std::string maxActiveStr = pn3MaxActiveKeyPointer.Get(configDocument)->GetString();
        EXPECT_EQ(maxActiveStr, "24h 0min 0s");
    }
};

TEST_F(PutRuntimeConfigHandlerTest, ActivatePn3)
{
    checkGetConfigPn3(false, {});

    {
        model::Timestamp expiry{model::Timestamp::now() + std::chrono::hours{2}};
        changePN3(true, expiry);
        checkGetConfigPn3(true, expiry);
    }

    // update expiry:
    {
        model::Timestamp expiry{model::Timestamp::now() + std::chrono::hours{15}};
        changePN3(true, expiry);
        checkGetConfigPn3(true, expiry);
    }

    // disable:
    {
        changePN3(false, {});
        checkGetConfigPn3(false, {});
    }
}
