/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include "shared/util/Base64.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"

#include <fstream>

using model::Communication;
using model::Bundle;
using model::Task;

class Erp5827Test : public ErpWorkflowTest
{
};

TEST_F(Erp5827Test, run)//NOLINT(readability-function-cognitive-complexity)
{
    std::string kvnrInsurant;
    generateNewRandomKVNR(kvnrInsurant);
    const std::string pharmacy1 = "3-SMC-B-Testkarte-883110000129068";
    const std::string pharmacy2 = "3-SMC-B-Testkarte-883110000129069";
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    std::string accessCode{ task->accessCode() };
    std::string qesBundle;
    ASSERT_NO_THROW(qesBundle = std::get<0>(makeQESBundle(kvnrInsurant, task->prescriptionId(), model::Timestamp::now())));
    ASSERT_FALSE(qesBundle.empty());
    ASSERT_NO_FATAL_FAILURE(task = taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode, qesBundle));
    ASSERT_TRUE(task.has_value());
    ASSERT_EQ(task->kvnr(), kvnrInsurant);
    std::optional<model::Communication> infoReq1;
    ASSERT_NO_FATAL_FAILURE(
        infoReq1 = communicationPost(model::Communication::MessageType::DispReq, task.value(), ActorRole::Insurant,
                                     kvnrInsurant, ActorRole::Pharmacists, pharmacy1,
                                     R"({"version":1,"supplyOptionsType":"onPremise","address":["zu hause"]})"));
    ASSERT_TRUE(infoReq1.has_value());
    std::optional<model::Communication> reply1;
    ASSERT_NO_FATAL_FAILURE(reply1 = communicationPost(
        model::Communication::MessageType::Reply, task.value(),
        ActorRole::Pharmacists, pharmacy1,
        ActorRole::Insurant, kvnrInsurant,
        R"({"version":1,"supplyOptionsType":"onPremise","info_text":"Nein"})"));
    ASSERT_TRUE(reply1.has_value());
    std::optional<model::Communication> infoReq2;
    ASSERT_NO_FATAL_FAILURE(
        infoReq2 = communicationPost(model::Communication::MessageType::DispReq, task.value(), ActorRole::Insurant,
                                     kvnrInsurant, ActorRole::Pharmacists, pharmacy2,
                                     R"({"version":1,"supplyOptionsType":"onPremise","address":["zu hause"]})"));
    ASSERT_TRUE(infoReq2.has_value());
    std::optional<model::Communication> reply2;
    ASSERT_NO_FATAL_FAILURE(reply2 = communicationPost(
        model::Communication::MessageType::Reply, task.value(),
        ActorRole::Pharmacists, pharmacy2,
        ActorRole::Insurant, kvnrInsurant,
        R"({"version":1,"supplyOptionsType":"onPremise","info_text":"Ja"})"));
    ASSERT_TRUE(reply2.has_value());
    const JWT jwtInsurant{ JwtBuilder::testBuilder().makeJwtVersicherter(kvnrInsurant) };
    model::Timestamp timestamp = model::Timestamp::now() + std::chrono::hours(-24);
    std::string sent = timestamp.toXsDateTimeWithoutFractionalSeconds();
    sent = String::replaceAll(sent, ":", "%3A");
    sent = String::replaceAll(sent, "+", "%2B");
    std::optional<Bundle> commBundle;
    ASSERT_NO_FATAL_FAILURE(commBundle =
        communicationsGet(jwtInsurant, "sender=" + pharmacy1 + "%2C" + pharmacy2 + "&sent=ge" + sent));
    ASSERT_TRUE(commBundle.has_value());
    std::vector<Communication> comms;
    EXPECT_NO_THROW(comms = commBundle->getResourcesByType<Communication>("Communication"));
    EXPECT_EQ(comms.size(), 2);
}
