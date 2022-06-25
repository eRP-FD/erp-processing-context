/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/model/Communication.hxx"

#include <gtest/gtest.h>

#include "ErpWorkflowTestFixture.hxx"

class Erp9230Test : public ErpWorkflowTest
{
};

TEST_F(Erp9230Test, PresetReceivedMustBeDiscarded)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));
    ASSERT_TRUE(prescriptionId.has_value());
    auto kvnr = generateNewRandomKVNR();
    ASSERT_NO_FATAL_FAILURE(
        taskActivate(*prescriptionId, accessCode, std::get<0>(makeQESBundle(kvnr, *prescriptionId, model::Timestamp::now()))));

    const auto jwtPharma = JwtBuilder::testBuilder().makeJwtApotheke();
    const auto telematikId = jwtPharma.stringForClaim(JWT::idNumberClaim);

    auto builder = CommunicationJsonStringBuilder(model::Communication::MessageType::InfoReq);
    builder.setPrescriptionId(prescriptionId->toString());
    builder.setAbout("about");
    builder.setPayload("info request");
    builder.setRecipient(ActorRole::Pharmacists, *telematikId);
    builder.setSender(ActorRole::Insurant, kvnr);
    builder.setTimeSent(model::Timestamp::now().toXsDateTime());
    auto someReceivedTime = model::Timestamp::fromXsDate("2020-02-02");
    builder.setTimeReceived(someReceivedTime.toXsDateTime());

    RequestArguments requestArguments(HttpMethod::POST, "/Communication", builder.createJsonString(),
                                      MimeType::fhirJson);
    auto jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    requestArguments.jwt = jwt;
    requestArguments.headerFields.emplace(Header::Authorization, getAuthorizationBearerValueForJwt(jwt));
    requestArguments.headerFields.emplace(Header::Accept, MimeType::fhirJson);
    requestArguments.headerFields.emplace(Header::XAccessCode, accessCode);
    requestArguments.expectedInnerStatus = HttpStatus::Created;
    ClientResponse serverResponse;
    ASSERT_NO_FATAL_FAILURE(std::tie(std::ignore, serverResponse) = send(requestArguments));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::Created);

    auto communicationEcho = model::Communication::fromJson(
        serverResponse.getBody(), *getJsonValidator(), *StaticData::getXmlValidator(),
        *StaticData::getInCodeValidator(),
        model::Communication::messageTypeToSchemaType(model::Communication::MessageType::InfoReq));

    // Before the fix this check failed:
    EXPECT_FALSE(communicationEcho.timeReceived().has_value());

    // Now retrieve the communication as pharmacist
    const auto beforeReception = model::Timestamp::now();
    std::optional<model::Bundle> communicationBundle;
    ASSERT_NO_FATAL_FAILURE(communicationBundle =
                                communicationsGet(jwtPharma, UrlHelper::escapeUrl("received=NULL&sender=" + kvnr)));

    // Before the fix 0 communications where retrieved here.
    EXPECT_EQ(communicationBundle->getResourceCount(), 1);
    const auto communications = communicationBundle->getResourcesByType<model::Communication>("Communication");
    ASSERT_EQ(communications.size(), 1);
    ASSERT_TRUE(communications[0].timeReceived().has_value());
    EXPECT_NE(*communications[0].timeReceived(), someReceivedTime);
    EXPECT_GT(*communications[0].timeReceived(), beforeReception);
}
