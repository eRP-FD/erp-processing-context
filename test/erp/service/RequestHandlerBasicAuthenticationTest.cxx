/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/admin/AdminRequestHandler.hxx"
#include "erp/enrolment/EnrolmentRequestHandler.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ErpMacros.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "test/util/StaticData.hxx"

#include <boost/mpl/map.hpp>
#include <gtest/gtest.h>

class MockRequestHandlerBasicAuthentication : public RequestHandlerBasicAuthentication
{
public:
    void handleRequest(SessionContext&) override
    {
    }
    Operation getOperation(void) const override
    {
        return Operation::UNKNOWN;
    }
};

class RequestHandlerBasicAuthenticationTest : public testing::Test
{
protected:

    void handleBasicAuthentication(SessionContext& session)
    {
        handler.handleBasicAuthentication(
            session, ConfigurationKey::ADMIN_CREDENTIALS);
    }

    const std::string auth = "c3VwZXJ1c2VyOnN1cGVyc2VjcmV0";
    EnvironmentVariableGuard adminApiCredentials{"ERP_ADMIN_CREDENTIALS", auth};
    MockRequestHandlerBasicAuthentication handler;
    PcServiceContext serviceContext{StaticData::makePcServiceContext()};
    Header header;
    ServerResponse response;
    AccessLog accessLog;
};

TEST_F(RequestHandlerBasicAuthenticationTest, CorrectCredentials)
{
    header.addHeaderField(Header::Authorization, "Basic " + auth);
    ServerRequest request{Header(header)};
    SessionContext session{serviceContext, request, response, accessLog};
    EXPECT_NO_THROW(handleBasicAuthentication(session));
}

TEST_F(RequestHandlerBasicAuthenticationTest, WrongMethod)
{
    header.addHeaderField(Header::Authorization, "Digest " + auth);
    ServerRequest request{Header()};
    SessionContext session{serviceContext, request, response, accessLog};
    EXPECT_ERP_EXCEPTION(handleBasicAuthentication(session), HttpStatus::Unauthorized);
}

TEST_F(RequestHandlerBasicAuthenticationTest, NoMethod)
{
    header.addHeaderField(Header::Authorization, auth);
    ServerRequest request{Header()};
    SessionContext session{serviceContext, request, response, accessLog};
    EXPECT_ERP_EXCEPTION(handleBasicAuthentication(session), HttpStatus::Unauthorized);
}

TEST_F(RequestHandlerBasicAuthenticationTest, WrongCredentials)
{
    header.addHeaderField(Header::Authorization, "Basic 123");
    ServerRequest request{Header()};
    SessionContext session{serviceContext, request, response, accessLog};
    EXPECT_ERP_EXCEPTION(handleBasicAuthentication(session), HttpStatus::Unauthorized);
}

TEST_F(RequestHandlerBasicAuthenticationTest, MissingAuthorizationHeader)
{
    ServerRequest request{Header()};
    SessionContext session{serviceContext, request, response, accessLog};
    EXPECT_ERP_EXCEPTION(handleBasicAuthentication(session), HttpStatus::Unauthorized);
}
