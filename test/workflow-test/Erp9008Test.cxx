#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include "test/util/ResourceManager.hxx"
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
};

TEST_F(Erp9008Test, run)//NOLINT(readability-function-cognitive-complexity)
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
    const auto [qesBundle, _] = makeQESBundle(kvnr, task->prescriptionId(), model::Timestamp::now());
    ASSERT_NO_FATAL_FAILURE(task = taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode, qesBundle));
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
