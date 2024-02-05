/*
* (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/ErpRequirements.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/model/Consent.hxx"
#include "erp/service/task/AcceptTaskHandler.hxx"
#include "erp/util/ByteHelper.hxx"
#include "test/erp/pc/CFdSigErpTestHelper.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <erp/model/OperationOutcome.hxx>
#include <erp/service/task/GetTaskHandler.hxx>
#include <erp/service/task/RejectTaskHandler.hxx>

class AcceptTaskTest : public EndpointHandlerTest
{
};

TEST_F(AcceptTaskTest, OwnerDeleted)
{
    const std::string validAccessCode = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";
    const auto id =
       model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4714).toString();
    std::string secret;
    {
        AcceptTaskHandler handler({});
        const Header requestHeader{HttpMethod::POST, "/Task/" + id + "/$accept/", 0, {}, HttpStatus::Unknown};
        ServerRequest serverRequest{Header(requestHeader)};
        serverRequest.setPathParameters({"id"}, {id});
        serverRequest.setQueryParameters({{"ac", validAccessCode}});
        serverRequest.setAccessToken(JwtBuilder::testBuilder().makeJwtApotheke());
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        auto bundle = model::Bundle::fromXmlNoValidation(serverResponse.getBody());
        auto acceptedTasks = bundle.getResourcesByType<model::Task>();
        ASSERT_EQ(acceptedTasks.size(), 1);
        EXPECT_TRUE(acceptedTasks[0].owner().has_value());
        ASSERT_TRUE(acceptedTasks[0].secret().has_value());
        secret = *acceptedTasks[0].secret();
    }

    {
        RejectTaskHandler rejectTaskHandler({});
        const Header requestHeader{HttpMethod::POST, "/Task/" + id + "/$reject/", 0, {}, HttpStatus::Unknown};
        ServerRequest serverRequest{Header(requestHeader)};
        serverRequest.setPathParameters({"id"}, {id});
        serverRequest.setQueryParameters({{"secret", secret}});
        serverRequest.setAccessToken(JwtBuilder::testBuilder().makeJwtApotheke());
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_NO_THROW(rejectTaskHandler.handleRequest(sessionContext));
    }

    GetTaskHandler getTaskHandler({});
    const Header requestHeader{HttpMethod::GET, "/Task/" + id, 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{Header(requestHeader)};
    serverRequest.setPathParameters({"id"}, {id});
    serverRequest.setQueryParameters({{"ac", validAccessCode}});
    serverRequest.setAccessToken(
        JwtBuilder::testBuilder().makeJwtVersicherter(std::string(ResourceTemplates::TaskOptions{}.kvnr)));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
    ASSERT_NO_THROW(getTaskHandler.handleRequest(sessionContext));

    auto bundle = model::Bundle::fromXmlNoValidation(serverResponse.getBody());
    auto tasks = bundle.getResourcesByType<model::Task>();
    ASSERT_EQ(tasks.size(), 1);
    // GEMREQ-start A_24175#test1
    A_24175.test("task.owner has been deleted");
    EXPECT_FALSE(tasks[0].owner().has_value());
    // GEMREQ-end A_24175#test1
    ASSERT_FALSE(tasks[0].secret().has_value());
}
