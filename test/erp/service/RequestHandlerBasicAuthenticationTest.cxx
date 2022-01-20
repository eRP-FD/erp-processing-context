/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/admin/AdminRequestHandler.hxx"
#include "erp/admin/AdminServiceContext.hxx"
#include "erp/enrolment/EnrolmentRequestHandler.hxx"
#include "erp/enrolment/EnrolmentServiceContext.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ErpMacros.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/server/request/ServerRequest.hxx"

#include <boost/mpl/map.hpp>
#include <gtest/gtest.h>

class MockRequestHandlerBasicAuthentication : public RequestHandlerBasicAuthentication<AdminServiceContext>
{
public:
    void handleRequest(SessionContext<AdminServiceContext>&) override
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

    void handleBasicAuthentication(SessionContext<AdminServiceContext>& session)
    {
        handler.handleBasicAuthentication(
            session, ConfigurationKey::ADMIN_CREDENTIALS,
            ConfigurationKey::DEBUG_DISABLE_ADMIN_AUTH);
    }

    const std::string auth = "c3VwZXJ1c2VyOnN1cGVyc2VjcmV0";
    EnvironmentVariableGuard enableAdminApiAuth{"DEBUG_DISABLE_ADMIN_AUTH", "false"};
    EnvironmentVariableGuard adminApiCredentials{"ERP_ADMIN_CREDENTIALS", auth};
    MockRequestHandlerBasicAuthentication handler;
    AdminServiceContext serviceContext;
    Header header;
    ServerResponse response;
    AccessLog accessLog;
};

TEST_F(RequestHandlerBasicAuthenticationTest, CorrectCredentials)
{
    header.addHeaderField(Header::Authorization, "Basic " + auth);
    ServerRequest request{Header(header)};
    SessionContext<AdminServiceContext> session{serviceContext, request, response, accessLog};
    EXPECT_NO_THROW(handleBasicAuthentication(session));
}

TEST_F(RequestHandlerBasicAuthenticationTest, WrongMethod)
{
    header.addHeaderField(Header::Authorization, "Digest " + auth);
    ServerRequest request{Header()};
    SessionContext<AdminServiceContext> session{serviceContext, request, response, accessLog};
    EXPECT_ERP_EXCEPTION(handleBasicAuthentication(session), HttpStatus::Unauthorized);
}

TEST_F(RequestHandlerBasicAuthenticationTest, NoMethod)
{
    header.addHeaderField(Header::Authorization, auth);
    ServerRequest request{Header()};
    SessionContext<AdminServiceContext> session{serviceContext, request, response, accessLog};
    EXPECT_ERP_EXCEPTION(handleBasicAuthentication(session), HttpStatus::Unauthorized);
}

TEST_F(RequestHandlerBasicAuthenticationTest, WrongCredentials)
{
    header.addHeaderField(Header::Authorization, "Basic 123");
    ServerRequest request{Header()};
    SessionContext<AdminServiceContext> session{serviceContext, request, response, accessLog};
    EXPECT_ERP_EXCEPTION(handleBasicAuthentication(session), HttpStatus::Unauthorized);
}

TEST_F(RequestHandlerBasicAuthenticationTest, MissingAuthorizationHeader)
{
    ServerRequest request{Header()};
    SessionContext<AdminServiceContext> session{serviceContext, request, response, accessLog};
    EXPECT_ERP_EXCEPTION(handleBasicAuthentication(session), HttpStatus::Unauthorized);
}

TEST_F(RequestHandlerBasicAuthenticationTest, DisabledAuthCheck)
{
    EnvironmentVariableGuard disableAdminApiAuth{"DEBUG_DISABLE_ADMIN_AUTH", "true"};
    ServerRequest request{Header()};
    SessionContext<AdminServiceContext> session{serviceContext, request, response, accessLog};
    EXPECT_NO_THROW(handleBasicAuthentication(session));
}
