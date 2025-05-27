/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "ErpWorkflowTestFixture.hxx"
#include "erp/model/Communication.hxx"
#include "shared/model/ResourceFactory.hxx"

#include <gtest/gtest.h>

class Erp9230Test : public ErpWorkflowTest
{
};

TEST_F(Erp9230Test, PresetReceivedMustBeDiscarded)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));
    ASSERT_TRUE(prescriptionId.has_value());
    auto kvnr = generateNewRandomKVNR().id();
    ASSERT_NO_FATAL_FAILURE(
        taskActivateWithOutcomeValidation(*prescriptionId, accessCode, std::get<0>(makeQESBundle(kvnr, *prescriptionId, model::Timestamp::now()))));

    const auto jwtPharma = JwtBuilder::testBuilder().makeJwtApotheke();
    const auto telematikId = jwtPharma.stringForClaim(JWT::idNumberClaim);

    auto builder = CommunicationJsonStringBuilder(model::Communication::MessageType::DispReq);
    builder.setPrescriptionId(prescriptionId->toString());
    builder.setPayload(R"({"version":1,"supplyOptionsType":"onPremise","address":["zu hause"]})");
    builder.setRecipient(ActorRole::Pharmacists, *telematikId);
    builder.setSender(ActorRole::Insurant, kvnr);
    builder.setTimeSent(model::Timestamp::now().toXsDateTime());
    auto someReceivedTime = model::Timestamp::fromXsDate("2020-02-02", model::Timestamp::UTCTimezone);
    builder.setTimeReceived(someReceivedTime.toXsDateTime());
    builder.setAccessCode("68d8e217665f2b47cd616f6ffe6b879e5b1395b86f6cc01d5e694985431bbb59");
    RequestArguments requestArguments(HttpMethod::POST, "/Communication", builder.createJsonString(),
                                      MimeType::fhirJson);
    auto jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    requestArguments.jwt = jwt;
    requestArguments.headerFields.emplace(Header::Authorization, getAuthorizationBearerValueForJwt(jwt));
    requestArguments.headerFields.emplace(Header::Accept, MimeType::fhirJson);
    requestArguments.headerFields.emplace(Header::XAccessCode, accessCode);
    requestArguments.expectedInnerStatus = HttpStatus::Created;
    requestArguments.overrideExpectedPrescriptionId = prescriptionId->toString();
    ClientResponse serverResponse;
    ASSERT_NO_FATAL_FAILURE(std::tie(std::ignore, serverResponse) = send(requestArguments));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::Created);

    auto communicationEcho =
        model::ResourceFactory<model::Communication>::fromJson(serverResponse.getBody(), *getJsonValidator(), {})
            .getValidated(model::ProfileType::GEM_ERP_PR_Communication_DispReq);

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
