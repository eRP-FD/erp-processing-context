/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include "erp/util/Base64.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"

#include <fstream>

using model::Communication;
using model::Bundle;
using model::Task;

class Erp5822Test : public ErpWorkflowTest
{
public:

    void SetUp() override//NOLINT(readability-function-cognitive-complexity)
    {
        if (!Configuration::instance().getOptionalBoolValue(ConfigurationKey::DEBUG_DISABLE_QES_ID_CHECK, false))
        {
            GTEST_SKIP_("disabled, because the QES Key check could not be disabled");
        }
        kvnr = jwtVersicherter().stringForClaim(JWT::idNumberClaim).value();
        telematikIdApotheke = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
        ASSERT_NO_FATAL_FAILURE(communicationDeleteAll(jwtVersicherter()));
        ASSERT_NO_FATAL_FAILURE(communicationDeleteAll(jwtApotheke()));
        ASSERT_NO_FATAL_FAILURE(task = taskCreate());
        ASSERT_TRUE(task.has_value());
        std::string accessCode{task->accessCode()};
        ASSERT_NO_FATAL_FAILURE(
            task = taskActivate(task->prescriptionId(), accessCode,
                                std::get<0>(makeQESBundle(kvnr, task->prescriptionId(), fhirtools::Timestamp::now()))));
        ASSERT_TRUE(task.has_value());
        std::optional<Communication> infoReq;
        ASSERT_NO_FATAL_FAILURE(infoReq = communicationPost(model::Communication::MessageType::InfoReq,
                        *task,
                        ActorRole::Insurant, kvnr,
                        ActorRole::Pharmacists, telematikIdApotheke,
                        "Hallo Apotheker"));
        ASSERT_TRUE(infoReq.has_value());
        std::optional<Communication> reply;
        ASSERT_NO_FATAL_FAILURE(reply = communicationPost(model::Communication::MessageType::Reply,
                        *task,
                        ActorRole::Pharmacists, telematikIdApotheke,
                        ActorRole::Insurant, kvnr,
                        "Hallo potentieller Kunde, PZN ist vorrätig"));
        ASSERT_TRUE(reply.has_value());
        std::optional<Communication> dispReq;
        ASSERT_NO_FATAL_FAILURE(dispReq = communicationPost(model::Communication::MessageType::DispReq,
                        *task,
                        ActorRole::Insurant, kvnr,
                        ActorRole::Pharmacists, telematikIdApotheke,
                        "Ich will bestellen und habe ein E-Rezept"));
        ASSERT_TRUE(dispReq.has_value());
    }

    void checkComms(const JWT& jwt, size_t expectedCount)//NOLINT(readability-function-cognitive-complexity)
    {
        auto user = jwt.stringForClaim(JWT::idNumberClaim).value();
        std::optional<Bundle> commBundle;
        ASSERT_NO_FATAL_FAILURE( commBundle = communicationsGet(jwt, "received=NULL"));
        ASSERT_TRUE(commBundle.has_value());
        std::vector<Communication> comms;
        if (expectedCount > 0)
        {
            EXPECT_NO_THROW(comms = commBundle->getResourcesByType<Communication>("Communication"));
        }
        else
        {
            EXPECT_THROW(comms = commBundle->getResourcesByType<Communication>("Communication"), model::ModelException);
        }
        EXPECT_EQ(comms.size(), expectedCount);
        for (const auto& comm: comms)
        {
            ASSERT_TRUE(comm.sender().has_value());
            EXPECT_TRUE(comm.sender() == kvnr || comm.sender() == telematikIdApotheke);
            ASSERT_TRUE(comm.recipient().has_value());
            EXPECT_TRUE(comm.recipient() == kvnr || comm.recipient() == telematikIdApotheke);
            EXPECT_TRUE(comm.recipient() == user || comm.sender() == user);
            bool isRecipient = (comm.recipient() == user);
            EXPECT_EQ(isRecipient, comm.timeReceived().has_value()) << "Only Pharmacy retrieved messages";
        }
    }

    void TearDown() override//NOLINT(readability-function-cognitive-complexity)
    {
        ASSERT_NO_FATAL_FAILURE(communicationDeleteAll(jwtVersicherter()));
        ASSERT_NO_FATAL_FAILURE(communicationDeleteAll(jwtApotheke()));
        if (task)
        {
            std::string accessCode{task->accessCode()};
            auto secret = task->secret()?std::optional<std::string>{*task->secret()}:std::nullopt;
            taskAbort(task->prescriptionId(), jwtVersicherter(), accessCode, secret);
        }
    }

    JWT jwtFromResource(const std::string& fileName) const
    {
        rapidjson::Document claim;
        claim.CopyFrom(resourceManager.getJsonResource(testDataPath + fileName), claim.GetAllocator());
        return JwtBuilder::testBuilder().makeValidJwt(std::move(claim));
    }

    JWT jwtArzt() const override { return jwtFromResource("claims_arzt.json"); }
    JWT jwtApotheke() const override { return jwtFromResource("claims_apotheke.json"); }
    JWT jwtVersicherter() const override { return jwtFromResource("claims_versicherter.json"); }

    ResourceManager& resourceManager = ResourceManager::instance();
    const std::string testDataPath{"test/issues/ERP-5822/"};
    std::string kvnr;
    std::string telematikIdApotheke;
    std::optional<Task> task;
    EnvironmentVariableGuard environmentVariableGuard{"ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION", "false"};
    EnvironmentVariableGuard environmentVariableGuard2{"DEBUG_DISABLE_QES_ID_CHECK", "true"};
};

TEST_F(Erp5822Test, InsurantFirst)//NOLINT(readability-function-cognitive-complexity)
{
    ASSERT_NO_FATAL_FAILURE(checkComms(jwtVersicherter(), 3));
    ASSERT_NO_FATAL_FAILURE(checkComms(jwtVersicherter(), 2));
    ASSERT_NO_FATAL_FAILURE(checkComms(jwtApotheke(), 2));
    ASSERT_NO_FATAL_FAILURE(checkComms(jwtVersicherter(), 0));
    ASSERT_NO_FATAL_FAILURE(checkComms(jwtApotheke(), 0));
}

TEST_F(Erp5822Test, PharmacyFirst)//NOLINT(readability-function-cognitive-complexity)
{
    ASSERT_NO_FATAL_FAILURE(checkComms(jwtApotheke(), 3));
    ASSERT_NO_FATAL_FAILURE(checkComms(jwtApotheke(), 1));
    ASSERT_NO_FATAL_FAILURE(checkComms(jwtVersicherter(), 1));
    ASSERT_NO_FATAL_FAILURE(checkComms(jwtApotheke(), 0));
    ASSERT_NO_FATAL_FAILURE(checkComms(jwtVersicherter(), 0));
}
