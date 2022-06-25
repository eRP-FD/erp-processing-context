/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/ErpRequirements.hxx"
#include "erp/service/task/ActivateTaskHandler.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"

class ActivateTaskTest : public EndpointHandlerTest
{
public:
    void checkActivateTask(PcServiceContext& serviceContext, const std::string_view& taskJson,
                           const std::string_view& kbvBundleXml, const std::string_view& expectedKvnr,
                           const HttpStatus expectedStatus = HttpStatus::OK,
                           std::optional<model::Timestamp> signingTime = {})
    {
        auto cadesBesSignatureFile = CryptoHelper::toCadesBesSignature(std::string(kbvBundleXml), signingTime);
        const auto& origTask = model::Task::fromJsonNoValidation(taskJson);

        ActivateTaskHandler handler({});
        Header requestHeader{HttpMethod::POST,
                             "/Task/" + origTask.prescriptionId().toString() + "/$activate",
                             0,
                             {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                             HttpStatus::Unknown};
        requestHeader.addHeaderField("X-AccessCode", std::string(origTask.accessCode()));
        ServerRequest serverRequest{std::move(requestHeader)};

        std::string parameters = R"--(
<Parameters xmlns="http://hl7.org/fhir">
    <parameter>
        <name value="ePrescription" />
        <resource>
            <Binary>
                <contentType value="application/pkcs7-mime" />
                <data value=")--" +
                                 cadesBesSignatureFile + R"--("/>
            </Binary>
        </resource>
    </parameter>
</Parameters>)--";

        serverRequest.setBody(std::move(parameters));
        serverRequest.setPathParameters({"id"}, {origTask.prescriptionId().toString()});

        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        if (expectedStatus != HttpStatus::OK)
        {
            EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), expectedStatus);
            return;
        }

        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

        std::optional<model::Task> task;
        ASSERT_NO_THROW(task = model::Task::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator(),
                                                    *StaticData::getInCodeValidator(), SchemaType::Gem_erxTask));
        ASSERT_TRUE(task.has_value());
        EXPECT_EQ(task->prescriptionId(), origTask.prescriptionId());
        EXPECT_EQ(task->status(), model::Task::Status::ready);
        EXPECT_EQ(task->kvnr().value(), expectedKvnr);
        EXPECT_NO_THROW((void) task->expiryDate());
        EXPECT_FALSE(task->lastModifiedDate().toXsDateTime().empty());
        EXPECT_FALSE(task->authoredOn().toXsDateTime().empty());
        EXPECT_FALSE(task->accessCode().empty());
        EXPECT_FALSE(task->expiryDate().toXsDateTime().empty());
        EXPECT_FALSE(task->acceptDate().toXsDateTime().empty());
        EXPECT_TRUE(task->healthCarePrescriptionUuid().has_value());
        EXPECT_TRUE(task->patientConfirmationUuid().has_value());
        EXPECT_FALSE(task->receiptUuid().has_value());
    }
};

TEST_F(ActivateTaskTest, ActivateTask)
{
    auto& resourceManager = ResourceManager::instance();
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, resourceManager.getStringResource(dataPath + "/task3.json"),
                          resourceManager.getStringResource(dataPath + "/kbv_bundle.xml"), "X234567890", HttpStatus::OK,
                          model::Timestamp::fromXsDate("2021-06-08")));
}

TEST_F(ActivateTaskTest, ActivateTaskPkv)
{
    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50010);
    const char* const pkvKvnr = "X500000010";

    auto& resourceManager = ResourceManager::instance();
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext,
        replaceKvnr(
            replacePrescriptionId(resourceManager.getStringResource(dataPath + "/task_pkv_created_template.json"),
                                  pkvTaskId.toString()),
            pkvKvnr),
        replaceKvnr(replacePrescriptionId(resourceManager.getStringResource(dataPath + "/kbv_pkv_bundle_template.xml"),
                                          pkvTaskId.toString()),
                    pkvKvnr),
        pkvKvnr, HttpStatus::OK, model::Timestamp::fromXsDate("2021-06-08")));
}

TEST_F(ActivateTaskTest, ActivateTaskPkvInvalidCoverage)
{
    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50010);
    const char* const pkvKvnr = "X500000010";

    auto& resourceManager = ResourceManager::instance();
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext,
        replaceKvnr(
            replacePrescriptionId(resourceManager.getStringResource(dataPath + "/task_pkv_created_template.json"),
                                  pkvTaskId.toString()),
            pkvKvnr),
        replaceKvnr(replacePrescriptionId(resourceManager.getStringResource(dataPath + "/kbv_gkv_bundle_template.xml"),
                                          pkvTaskId.toString()),
                    pkvKvnr),
        pkvKvnr, HttpStatus::BadRequest, model::Timestamp::fromXsDate("2021-06-08")));
}

TEST_F(ActivateTaskTest, ActivateTaskBrokenSignature)
{
    auto& resourceManager = ResourceManager::instance();
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext,
                                              resourceManager.getStringResource(dataPath + "/task3.json"),
                                              "",// empty signature
                                              "X234567890", HttpStatus::BadRequest));
}

TEST_F(ActivateTaskTest, AuthoredOnSignatureDateEquality)
{
    A_22487.test("Task aktivieren - Prüfregel Ausstellungsdatum");
    auto& resourceManager = ResourceManager::instance();
    auto task = resourceManager.getStringResource(dataPath + "/task3.json");
    auto bundleTemplate = resourceManager.getStringResource(dataPath + "/kbv_bundle_authoredOn_template.xml");
    auto time1 = model::Timestamp::fromXsDateTime("2022-05-26T14:33:00+02:00");
    auto time2 = model::Timestamp::fromXsDateTime("2022-05-25T14:33:00+02:00");
    auto time3 = model::Timestamp::fromXsDateTime("2022-05-26T12:33:00+02:00");
    auto bundle1 = String::replaceAll(bundleTemplate, "###AUTHORED_ON###", time1.toXsDate());
    auto bundle2 = String::replaceAll(bundleTemplate, "###AUTHORED_ON###", time2.toXsDate());
    auto bundle3 = String::replaceAll(bundleTemplate, "###AUTHORED_ON###", time3.toXsDate());
    checkActivateTask(mServiceContext, task, bundle1, "X234567890", HttpStatus::OK, time1);
    checkActivateTask(mServiceContext, task, bundle1, "X234567890", HttpStatus::BadRequest, time2);
    checkActivateTask(mServiceContext, task, bundle1, "X234567890", HttpStatus::OK, time3);
    checkActivateTask(mServiceContext, task, bundle2, "X234567890", HttpStatus::BadRequest, time1);
    checkActivateTask(mServiceContext, task, bundle2, "X234567890", HttpStatus::OK, time2);
    checkActivateTask(mServiceContext, task, bundle2, "X234567890", HttpStatus::BadRequest, time3);
    checkActivateTask(mServiceContext, task, bundle3, "X234567890", HttpStatus::OK, time1);
    checkActivateTask(mServiceContext, task, bundle3, "X234567890", HttpStatus::BadRequest, time2);
    checkActivateTask(mServiceContext, task, bundle3, "X234567890", HttpStatus::OK, time3);

    EnvironmentVariableGuard environmentVariableGuard3("ERP_SERVICE_TASK_ACTIVATE_AUTHORED_ON_MUST_EQUAL_SIGNING_DATE",
                                                       "false");
    checkActivateTask(mServiceContext, task, bundle1, "X234567890", HttpStatus::OK, time1);
    checkActivateTask(mServiceContext, task, bundle1, "X234567890", HttpStatus::OK, time2);
    checkActivateTask(mServiceContext, task, bundle1, "X234567890", HttpStatus::OK, time3);
    checkActivateTask(mServiceContext, task, bundle2, "X234567890", HttpStatus::OK, time1);
    checkActivateTask(mServiceContext, task, bundle2, "X234567890", HttpStatus::OK, time2);
    checkActivateTask(mServiceContext, task, bundle2, "X234567890", HttpStatus::OK, time3);
    checkActivateTask(mServiceContext, task, bundle3, "X234567890", HttpStatus::OK, time1);
    checkActivateTask(mServiceContext, task, bundle3, "X234567890", HttpStatus::OK, time2);
    checkActivateTask(mServiceContext, task, bundle3, "X234567890", HttpStatus::OK, time3);
}
