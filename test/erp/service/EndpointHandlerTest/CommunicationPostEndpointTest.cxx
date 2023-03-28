/*
 * (C) Copyright IBM Deutschland GmbH 2023
 * (C) Copyright IBM Corp. 2023
 */
#include <gtest/gtest.h>

#include "erp/model/Communication.hxx"
#include "erp/pc/telematic_pseudonym/TelematicPseudonymManager.hxx"
#include "erp/service/CommunicationPostHandler.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"
#include "test/util/JsonTestUtils.hxx"



class CommunicationPostEndpointTest : public EndpointHandlerTest
{
};


TEST_F(CommunicationPostEndpointTest, ERP_12846_SubscriptionKeyRefresh)
{
    static const std::string kvnr{"X123456789"};
    auto body = CommunicationJsonStringBuilder{model::Communication::MessageType::InfoReq}
                    .setPrescriptionId(model::PrescriptionId::fromDatabaseId(
                                           model::PrescriptionType::apothekenpflichigeArzneimittel, 4711)
                                           .toString())
                    .setSender(ActorRole::Insurant, kvnr)
                    .setRecipient(ActorRole::Doctor, "A-Doctor")
                    .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
                    .setPayload("Haben sie das da?")
                    .createJsonString();
    CommunicationPostHandler handler{};
    // invalidate all keys to force refresh during `POST /Communication`
    auto& telematicPseudonymManager = mServiceContext.getTelematicPseudonymManager();
    telematicPseudonymManager.mKey1Start = {};
    telematicPseudonymManager.mKey1End = {};
    telematicPseudonymManager.mKey2Start = {};
    telematicPseudonymManager.mKey2End = {};
    Header requestHeader{HttpMethod::POST,
                         "/Communication",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setAccessToken(JwtBuilder::testBuilder().makeJwtVersicherter(kvnr));
    serverRequest.setBody(std::move(body));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::Created);
}

