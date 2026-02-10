/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/OuterResponseErrorData.hxx"
#include "fhirtools/converter/FhirConverter.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/erp-serverinfo.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/Resource.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <date/tz.h>
#include <thread>


TEST_P(ErpWorkflowTestP, Communications_GetById) // NOLINT
{
    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    std::optional<model::PrescriptionId> prescriptionId1;
    std::string accessCode1;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId1, accessCode1, GetParam()));

    std::string qesBundle1;
    std::vector<model::Communication> communications1;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle1, communications1, *prescriptionId1, kvnr, accessCode1));

    int expected = 2;
    switch (GetParam())
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::digitaleGesundheitsanwendungen:
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case model::PrescriptionType::tRezept:
            break;
        case model::PrescriptionType::direkteZuweisung:
        case model::PrescriptionType::direkteZuweisungPkv:
            expected = 1;
            break;
    }
    auto jwt = isDiga(GetParam()) ? jwtKostentraeger() : jwtApotheke();
    {
        std::optional<model::Bundle> communicationsBundle;
        ASSERT_NO_FATAL_FAILURE(communicationsBundle = communicationsGet(jwt, "identifier=gt2999-12-31"));
        ASSERT_EQ(communicationsBundle->getResourceCount(), 0);
    }

    {
        std::optional<model::Bundle> communicationsBundle;
        ASSERT_NO_FATAL_FAILURE(
            communicationsBundle = communicationsGet(
                jwt, "identifier=eq" + model::Timestamp::now().toGermanDate()));
        ASSERT_GE(communicationsBundle->getResourceCount(), expected);
    }

    ASSERT_NO_FATAL_FAILURE(checkTaskAbort(*prescriptionId1, jwtArzt(), kvnr, accessCode1, {}, communications1));
}

TEST_P(ErpWorkflowTestP, SearchCommunicationsByReceivedTimeRange) // NOLINT
{
    using namespace std::chrono_literals;

    // invoke POST /task/$create
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(GetParam()));
    ASSERT_TRUE(task);

    std::string kvnr;
    ASSERT_NO_FATAL_FAILURE(generateNewRandomKVNR(kvnr));

    std::optional<model::PrescriptionId> prescriptionId = task->prescriptionId();
    ASSERT_TRUE(prescriptionId);
    std::string qesBundle;
    ASSERT_NO_THROW(qesBundle = std::get<0>(makeQESBundle(kvnr, *prescriptionId, model::Timestamp::now())));

    // invoke /task/<id>/$activate
    ASSERT_NO_FATAL_FAILURE(task = taskActivateWithOutcomeValidation(*prescriptionId, std::string(task->accessCode()), qesBundle));
    ASSERT_TRUE(task);

    auto jwt = isDiga(GetParam()) ? jwtKostentraeger() : jwtApotheke();
    auto telematicId = jwt.stringForClaim(JWT::idNumberClaim);
    ASSERT_TRUE(telematicId.has_value());

    std::optional<model::Communication> communicationResponseA;
    std::string dispReqPayload =
        isDiga(task->type())
            ? ""
            : R"({"version": 1, "supplyOptionsType": "onPremise", "hint": "Ist das Medikament A bei Ihnen vorrätig?"})";
    ASSERT_NO_FATAL_FAILURE(communicationResponseA = communicationPost(
                                model::Communication::MessageType::DispReq, task.value(), ActorRole::Insurant, kvnr,
                                ActorRole::Pharmacists, telematicId.value(), dispReqPayload));

    std::this_thread::sleep_for(1s);

    std::optional<model::Communication> communicationResponseB;
    std::string dispReqPayloadB =
        isDiga(task->type())
            ? ""
            : R"({"version": 1, "supplyOptionsType": "onPremise", "hint": "Ist das Medikament B bei Ihnen vorrätig?"})";
    ASSERT_NO_FATAL_FAILURE(communicationResponseB = communicationPost(
                                model::Communication::MessageType::DispReq, task.value(), ActorRole::Insurant, kvnr,
                                ActorRole::Pharmacists, telematicId.value(), dispReqPayloadB));

    std::this_thread::sleep_for(2s);

    ASSERT_TRUE(communicationResponseA->timeSent().has_value());
    auto args = "sent=ge" + communicationResponseA->timeSent()
                                ->toXsDateTimeWithoutFractionalSeconds(model::Timestamp::GermanTimezone)
                                .substr(0, 19);
    std::optional<model::Bundle> communicationsBundle;
    ASSERT_NO_FATAL_FAILURE(communicationsBundle = communicationsGet(jwt, args)); // sets "received" timstamp
    EXPECT_EQ(communicationsBundle->getResourceCount(), 2);

    auto communications =  communicationsBundle->getResourcesByType<model::Communication>("Communication");
    ASSERT_EQ(communications.size(), 2);
    ASSERT_TRUE(communications[0].timeReceived().has_value());
    const auto start = communications[0].timeReceived().value() + (-1s);

    std::optional<model::Communication> communicationResponseC;
    std::string dispReqPayloadC =
        isDiga(task->type())
            ? ""
            : R"({"version": 1, "supplyOptionsType": "onPremise", "hint": "Ist das Medikament C bei Ihnen vorrätig?"})";
    ASSERT_NO_FATAL_FAILURE(communicationResponseC = communicationPost(
                                model::Communication::MessageType::DispReq, task.value(), ActorRole::Insurant, kvnr,
                                ActorRole::Pharmacists, telematicId.value(), dispReqPayloadC));
    std::optional<model::Communication> communicationResponseD;
    std::string dispReqPayloadD =
        isDiga(task->type())
            ? ""
            : R"({"version": 1, "supplyOptionsType": "onPremise", "hint": "Ist das Medikament D bei Ihnen vorrätig?"})";
    ASSERT_NO_FATAL_FAILURE(communicationResponseD = communicationPost(
                                model::Communication::MessageType::DispReq, task.value(), ActorRole::Insurant, kvnr,
                                ActorRole::Pharmacists, telematicId.value(), dispReqPayloadD));

    std::this_thread::sleep_for(1s);

    ASSERT_TRUE(communicationResponseC->timeSent().has_value());
    args = "sent=ge" + communicationResponseC->timeSent()
                           ->toXsDateTimeWithoutFractionalSeconds(model::Timestamp::GermanTimezone)
                           .substr(0, 19);
    ASSERT_NO_FATAL_FAILURE(communicationsBundle = communicationsGet(jwt, args)); // sets "received" timstamp

    communications =  communicationsBundle->getResourcesByType<model::Communication>("Communication");
    ASSERT_EQ(communications.size(), 2);
    ASSERT_TRUE(communications[0].timeReceived().has_value());
    const auto end = communications[0].timeReceived().value();

    EXPECT_EQ(communicationsBundle->getResourceCount(), 2);

    args = "received=gt" + start.toXsDateTimeWithoutFractionalSeconds(model::Timestamp::GermanTimezone).substr(0, 19) +
           "&received=lt" + end.toXsDateTimeWithoutFractionalSeconds(model::Timestamp::GermanTimezone).substr(0, 19) +
           "&_sort=sent";
    ASSERT_NO_FATAL_FAILURE(communicationsBundle = communicationsGet(jwt, args));
    ASSERT_EQ(communicationsBundle->getResourceCount(), 2);
    communications =  communicationsBundle->getResourcesByType<model::Communication>("Communication");
    ASSERT_EQ(communications.size(), 2);
    EXPECT_EQ(communications[0].id(), communicationResponseA->id());
    EXPECT_EQ(communications[1].id(), communicationResponseB->id());
}

INSTANTIATE_TEST_SUITE_P(ErpWorkflowCommunicationTestPInst, ErpWorkflowTestP,
                         testing::ValuesIn(testutils::allPrescriptionTypes()));

TEST_F(ErpWorkflowTest, CommunicationJsonValidationError)
{
    std::string communication =
        R"___({"resourceType":"Communication","meta":{"lastUpdated":null,"profile":["https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_DispReq"]},"basedOn":[{"reference":"Task/160.000.226.093.129.57/$accept?ac=5edb4d3d90e21fbba2b72855e851e8e5dc7ca1ef470a727103d069627687c8b4","type":null,"identifier":null,"display":null}],"status":"unknown","recipient":[{"reference":null,"type":null,"identifier":{"use":null,"type":null,"system":"https://gematik.de/fhir/sid/telematik-id","value":"3-10.2.0110201000.579","period":null},"display":null}],"identifier":{"use":null,"type":null,"system":"https://gematik.de/fhir/NamingSystem/OrderID","value":"6e59e060-0131-4020-ad05-1b4196a75f5e","period":null},"contained":[],"payload":[{"extension":null,"contentString":"{\"supplyOptionsType\":\"onPremise\",\"hint\":\"\"}"}]})___";
    RequestArguments reqArgs(HttpMethod::POST, "/Communication", communication, "application/json");
    reqArgs.overrideExpectedWorkflowVersion = "XXX";
    auto [outerResonse, innerResponse] = send(reqArgs);
    checkOperationOutcome(
        operationOutcomeFromResponse(innerResponse.getBody(), true), model::OperationOutcome::Issue::Type::invalid,
        "validation of JSON document failed. FHIR JSON schema can be retrieved here: "
        "https://hl7.org/fhir/R4/fhir.schema.json.zip"/*,
        "{\"expected\":[\"string\"],\"actual\":\"null\",\"errorCode\":20,\"instanceRef\":\"#/meta/"
        "lastUpdated\",\"schemaRef\":\"/home/jens/erp/erp-processing-context/cmake-build-debug-wsl-gcc-12/bin/"
        "resources/schema/hl7.fhir.r4.core/4.0.1/json/fhir.json#/definitions/instant\"}"*/);
}

TEST_F(ErpWorkflowTest, CommunicationPayloadJsonValidationError)
{
    auto task = taskCreate();
    auto kvnr = generateNewRandomKVNR();
    taskActivate(task->prescriptionId(), task->accessCode(),
                 std::get<0>(makeQESBundle(kvnr.id(), task->prescriptionId(), model::Timestamp::now())));
    CommunicationJsonStringBuilder builder(model::Communication::MessageType::DispReq,
                                           ResourceTemplates::Versions::GEM_ERP_current());
    builder.setPayload(
        R"___({ "version": 1, "supplyOptionsType": "invalid", "name": "Dr. Maximilian von Muster", "address": [ "wohnhaft bei Emilia Fischer", "Bundesallee 312", "123. OG", "12345 Berlin" ], "hint": "Bitte im Morsecode klingeln: -.-.", "phone": "004916094858168" })___");
    builder.setPrescriptionId(task->prescriptionId().toString());
    builder.setRecipient(ActorRole::Pharmacists, "3-10.2.0110201000.579");
    builder.setAccessCode(std::string{task->accessCode()});
    auto communication = builder.createJsonString();
    auto reqArgs = RequestArguments(HttpMethod::POST, "/Communication", communication, "application/json")
                       .withHeader(Header::XAccessCode, std::string{task->accessCode()});
    reqArgs.overrideExpectedPrescriptionId = task->prescriptionId().toString();
    auto [outerResonse, innerResponse] = send(reqArgs);
    checkOperationOutcome(
        operationOutcomeFromResponse(innerResponse.getBody(), true), model::OperationOutcome::Issue::Type::invalid,
        "Invalid payload: does not conform to expected JSON schema: validation of JSON document failed"/*,
        "{\"enum\":{\"errorCode\":19,\"instanceRef\":\"#/supplyOptionsType\",\"schemaRef\":\"/home/jens/erp/"
        "erp-processing-context/cmake-build-debug-wsl-gcc-12/bin/resources/schema/shared/json/"
        "CommunicationDispReqPayload.json#/properties/supplyOptionsType\"}}"*/);
}

TEST_F(ErpWorkflowTest, CommunicationDigaReject)
{
    if (ResourceTemplates::Versions::GEM_ERP_current() < ResourceTemplates::Versions::GEM_ERP_1_5_2)
    {
        GTEST_SKIP();
    }
    auto task = taskCreate();
    auto kvnr = generateNewRandomKVNR();
    taskActivate(task->prescriptionId(), task->accessCode(), std::get<0>(makeQESBundle(kvnr.id(), task->prescriptionId(), model::Timestamp::now())));
    CommunicationJsonStringBuilder builder(model::Communication::MessageType::DiGA,
                                           ResourceTemplates::Versions::GEM_ERP_current());
    builder.setPayload("Nachrichteninhalt für den Versicherten");
    builder.setPrescriptionId(task->prescriptionId().toString());
    builder.setSender(ActorRole::Pharmacists, "3-10.2.0110201000.579");
    builder.setRecipient(ActorRole::Insurant, kvnr.id());
    auto communication = builder.createXmlString();
    auto reqArgs = RequestArguments(HttpMethod::POST, "/Communication", communication, "application/fhir+xml")
                       .withJwt(jwtKostentraeger());
    reqArgs.overrideExpectedPrescriptionId = task->prescriptionId().toString();
    auto [outerResonse, innerResponse] = send(reqArgs);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::Created);

    std::optional<model::Bundle> communicationsBundle;
    ASSERT_NO_THROW(communicationsBundle = communicationsGet(JwtBuilder::testBuilder().makeJwtVersicherter(kvnr.id())));
    ASSERT_TRUE(communicationsBundle.has_value());
    auto communications = communicationsBundle->getResourcesByType<model::Communication>();
    ASSERT_EQ(communications.size(), 1);
    EXPECT_EQ(communications[0].messageType(), model::Communication::MessageType::DiGA);
    auto payload = communications[0].contentString();
    ASSERT_TRUE(payload.has_value());
    EXPECT_EQ(*payload, "Nachrichteninhalt für den Versicherten");
}
