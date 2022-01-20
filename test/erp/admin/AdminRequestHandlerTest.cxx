/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/admin/AdminRequestHandler.hxx"
#include "erp/admin/AdminServiceContext.hxx"
#include "erp/common/Header.hxx"
#include "erp/server/AccessLog.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "test/mock/MockTerminationHandler.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "erp/admin/AdminServer.hxx"

#include <gtest/gtest.h>

class AdminRequestHandlerTest : public testing::Test
{
public:
    AdminRequestHandlerTest()
    {
        MockTerminationHandler::setupForTesting();
        serviceContext.setIoContext(&mIoContext);
        header.addHeaderField(Header::ContentType, ContentMimeType::xWwwFormUrlEncoded);
    }

public:
    ~AdminRequestHandlerTest()
    {
        // reset state to running for other tests.
        MockTerminationHandler::setupForTesting();
    }

    boost::asio::io_context mIoContext;
    EnvironmentVariableGuard disableAdminApiAuth{"DEBUG_DISABLE_ADMIN_AUTH", "true"};
    EnvironmentVariableGuard defaultTimeout{"ERP_ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS", "2"};
    AdminServiceContext serviceContext;
    Header header;
    ServerResponse response;
    AccessLog accessLog;
};


TEST_F(AdminRequestHandlerTest, SuccessfulShutdownDefaultDelay)
{
    ServerRequest request{Header(header)};
    SessionContext<AdminServiceContext> session{serviceContext, request, response, accessLog};
    PostRestartHandler restartHandler;
    EXPECT_NO_THROW(restartHandler.handleRequest(session));
    EXPECT_EQ(session.response.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(session.response.getBody(), "shutdown in 2 seconds");
}

TEST_F(AdminRequestHandlerTest, SuccessfulShutdownParameterDelay)
{
    ServerRequest request{Header(header)};
    request.setBody("delay-seconds=3");
    SessionContext<AdminServiceContext> session{serviceContext, request, response, accessLog};
    PostRestartHandler restartHandler;
    EXPECT_NO_THROW(restartHandler.handleRequest(session));
    EXPECT_EQ(session.response.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(session.response.getBody(), "shutdown in 3 seconds");
}

TEST_F(AdminRequestHandlerTest, SuccessfulShutdownParameterImmediate)
{
    ServerRequest request{Header(header)};
    request.setBody("delay-seconds=0");
    SessionContext<AdminServiceContext> session{serviceContext, request, response, accessLog};
    PostRestartHandler restartHandler;
    EXPECT_NO_THROW(restartHandler.handleRequest(session));
    EXPECT_EQ(session.response.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(session.response.getBody(), "shutdown in 0 seconds");
}

TEST_F(AdminRequestHandlerTest, FailedShutdownWrongParameter)
{
    ServerRequest request{Header(header)};
    request.setBody("delay-second=3");
    SessionContext<AdminServiceContext> session{serviceContext, request, response, accessLog};
    PostRestartHandler restartHandler;
    EXPECT_NO_THROW(restartHandler.handleRequest(session));
    EXPECT_EQ(session.response.getHeader().status(), HttpStatus::BadRequest);
}
TEST_F(AdminRequestHandlerTest, FailedShutdownWrongParameter2)
{
    ServerRequest request{Header(header)};
    request.setBody("delay-seconds=3;delay-second=3");
    SessionContext<AdminServiceContext> session{serviceContext, request, response, accessLog};
    PostRestartHandler restartHandler;
    EXPECT_NO_THROW(restartHandler.handleRequest(session));
    EXPECT_EQ(session.response.getHeader().status(), HttpStatus::BadRequest);
}

TEST_F(AdminRequestHandlerTest, FailedShutdownParameterRange)
{
    ServerRequest request{Header(header)};
    request.setBody("delay-seconds=-1");
    SessionContext<AdminServiceContext> session{serviceContext, request, response, accessLog};
    PostRestartHandler restartHandler;
    EXPECT_NO_THROW(restartHandler.handleRequest(session));
    EXPECT_EQ(session.response.getHeader().status(), HttpStatus::BadRequest);
}