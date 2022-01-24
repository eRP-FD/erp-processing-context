#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include "tools/ResourceManager.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"


class Erp9008Test : public ErpWorkflowTest
{
    void SetUp() override
    {
        auto validator = std::make_shared<XmlValidatorStatic>();
        client.reset();
        client = TestClient::create(std::shared_ptr<XmlValidator>(validator, &validator->mXmlValidator));
        ErpWorkflowTest::SetUp();
    }
    EnvironmentVariableGuard ERP_FHIR_PROFILE_OLD_VALID_UNTIL
        {"ERP_FHIR_PROFILE_OLD_VALID_UNTIL", "2020-01-01T00:00:00+01:00"};
    EnvironmentVariableGuard ERP_FHIR_PROFILE_VALID_FROM
        {"ERP_FHIR_PROFILE_VALID_FROM", "2020-01-01T00:00:00+01:00"};

};

TEST_F(Erp9008Test, run)
{
    using namespace std::string_literals;
    auto& resourceManager = ResourceManager::instance();
    auto commReply = resourceManager.getStringResource("test/issues/ERP-9008/CommunicationReply_noVersion.xml");
    std::optional<model::Task> task;
    std::string kvnr;
    ASSERT_NO_FATAL_FAILURE(generateNewRandomKVNR(kvnr));
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    std::string accessCode{task->accessCode()};
    auto qesBundle = makeQESBundle(kvnr, task->prescriptionId(), model::Timestamp::now());
    ASSERT_NO_FATAL_FAILURE(task = taskActivate(task->prescriptionId(), accessCode, qesBundle));
    auto path = "/Communication/" + task->prescriptionId().toString();
    auto apothekenJwt = jwtApotheke();
    auto apothekenId = apothekenJwt.stringForClaim(JWT::idNumberClaim);
    ASSERT_TRUE(apothekenId.has_value());
    commReply = regex_replace(commReply, std::regex{"###KVNR###"}, kvnr);
    commReply = regex_replace(commReply, std::regex{"###TELEMATIK_ID###"}, *apothekenId);
    commReply = regex_replace(commReply, std::regex{"###TASK_ID###"}, task->prescriptionId().toString());
    std::optional<std::tuple<ClientResponse, ClientResponse>> responses;
    ASSERT_NO_FATAL_FAILURE(
        responses = send(RequestArguments(HttpMethod::POST, "/Communication", commReply, ContentMimeType::fhirXmlUtf8)
        .withJwt(apothekenJwt))
    );
    ASSERT_TRUE(responses.has_value());
    auto& [outer, inner] = *responses;
    EXPECT_EQ(outer.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(inner.getHeader().status(), HttpStatus::Created);
}
