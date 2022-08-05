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
    std::string requestBody(const std::string_view& kbvBundleXml, std::optional<model::Timestamp> signingTime)
    {
        auto cadesBesSignatureFile = CryptoHelper::toCadesBesSignature(std::string(kbvBundleXml), signingTime);
        return R"--(
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
    }

    ServerRequest serverRequest(const std::string_view& taskJson, const std::string_view& kbvBundleXml,
                                std::optional<model::Timestamp> signingTime)
    {
        const auto& origTask = model::Task::fromJsonNoValidation(taskJson);

        ActivateTaskHandler handler({});
        Header requestHeader{HttpMethod::POST,
                             "/Task/" + origTask.prescriptionId().toString() + "/$activate",
                             0,
                             {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                             HttpStatus::Unknown};
        requestHeader.addHeaderField("X-AccessCode", std::string(origTask.accessCode()));
        ServerRequest serverRequest{std::move(requestHeader)};
        serverRequest.setBody(requestBody(kbvBundleXml, signingTime));
        serverRequest.setPathParameters({"id"}, {origTask.prescriptionId().toString()});
        return serverRequest;
    }

    void checkActivateTask(PcServiceContext& serviceContext, const std::string_view& taskJson,
                           const std::string_view& kbvBundleXml, const std::string_view& expectedKvnr,
                           const HttpStatus expectedStatus = HttpStatus::OK,
                           std::optional<model::Timestamp> signingTime = {}, bool insertTask = false)
    {
        const auto& origTask = model::Task::fromJsonNoValidation(taskJson);

        ActivateTaskHandler handler({});
        ServerRequest request{serverRequest(taskJson, kbvBundleXml, signingTime)};
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{serviceContext, request, serverResponse, accessLog};
        if (insertTask)
        {
            auto& db = dynamic_cast<MockDatabase&>(sessionContext.database()->getBackend());
            db.insertTask(origTask);
        }

        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        if (expectedStatus != HttpStatus::OK && expectedStatus != HttpStatus::Accepted)
        {
            EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), expectedStatus);
            return;
        }

        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        ASSERT_EQ(serverResponse.getHeader().status(), expectedStatus);

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
    auto time1 = model::Timestamp::fromXsDateTime("2022-06-25T22:33:00+00:00");
    const auto* date1 = "2022-06-26";
    auto time2 = model::Timestamp::fromXsDateTime("2022-06-25T00:33:00+00:00");
    const auto* date2 = "2022-06-25";
    auto time3 = model::Timestamp::fromXsDateTime("2022-06-26T12:33:00+00:00");
    const auto* date3 = "2022-06-26";
    auto time4 = model::Timestamp::fromXsDateTime("2022-01-25T23:33:00+00:00");
    const auto* date4 = "2022-01-26";
    auto time5 = model::Timestamp::fromXsDateTime("2022-01-25T22:33:00+00:00");
    const auto* date5 = "2022-01-25";
    auto bundle1 = String::replaceAll(bundleTemplate, "###AUTHORED_ON###", date1);
    auto bundle2 = String::replaceAll(bundleTemplate, "###AUTHORED_ON###", date2);
    auto bundle3 = String::replaceAll(bundleTemplate, "###AUTHORED_ON###", date3);
    auto bundle4 = String::replaceAll(bundleTemplate, "###AUTHORED_ON###", date4);
    auto bundle5 = String::replaceAll(bundleTemplate, "###AUTHORED_ON###", date5);
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle1, "X234567890", HttpStatus::OK, time1));
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, bundle1, "X234567890", HttpStatus::BadRequest, time2));
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle1, "X234567890", HttpStatus::OK, time3));
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, bundle2, "X234567890", HttpStatus::BadRequest, time1));
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle2, "X234567890", HttpStatus::OK, time2));
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, bundle2, "X234567890", HttpStatus::BadRequest, time3));
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle3, "X234567890", HttpStatus::OK, time1));
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, bundle3, "X234567890", HttpStatus::BadRequest, time2));
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle3, "X234567890", HttpStatus::OK, time3));
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle4, "X234567890", HttpStatus::OK, time4));
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle5, "X234567890", HttpStatus::OK, time5));
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, bundle5, "X234567890", HttpStatus::BadRequest, time4));
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, bundle4, "X234567890", HttpStatus::BadRequest, time5));

    EnvironmentVariableGuard environmentVariableGuard3("ERP_SERVICE_TASK_ACTIVATE_AUTHORED_ON_MUST_EQUAL_SIGNING_DATE",
                                                       "false");
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle1, "X234567890", HttpStatus::OK, time1));
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle1, "X234567890", HttpStatus::OK, time2));
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle1, "X234567890", HttpStatus::OK, time3));
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle2, "X234567890", HttpStatus::OK, time1));
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle2, "X234567890", HttpStatus::OK, time2));
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle2, "X234567890", HttpStatus::OK, time3));
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle3, "X234567890", HttpStatus::OK, time1));
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle3, "X234567890", HttpStatus::OK, time2));
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle3, "X234567890", HttpStatus::OK, time3));
}

TEST_F(ActivateTaskTest, Erp10633UnslicedExtension)
{
    EnvironmentVariableGuard onUnknownExtensionGuard{
            "ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION", "report"};
    auto& resourceManager = ResourceManager::instance();
    auto taskJson = resourceManager.getStringResource("test/EndpointHandlerTest/task_pkv_created_template.json");
    auto bundle = resourceManager.getStringResource("test/issues/ERP-10633/Step09.xml");
    taskJson = String::replaceAll(taskJson, "##PRESCRIPTION_ID##", "200.000.000.000.429.45");
    auto signingTime = model::Timestamp::fromGermanDate("2022-07-25");
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, taskJson, bundle, "Y229270213", HttpStatus::Accepted, signingTime, true));
}


class ActivateTaskValidationModeTest
    : public ActivateTaskTest
    , public ::testing::WithParamInterface<Configuration::GenericValidationMode>
{};

TEST_P(ActivateTaskValidationModeTest, OnUnexpectedKbvExtension)
{
    EnvironmentVariableGuard validationModeGuard{"ERP_SERVICE_GENERIC_VALIDATION_MODE",
                                                 std::string{magic_enum::enum_name(GetParam())}};
    A_22927.test("E-Rezept-Fachdienst - Task aktivieren - Ausschluss unspezifizierter Extensions");
    auto& resourceManager = ResourceManager::instance();
    const auto& task = resourceManager.getStringResource(dataPath + "/task3.json");
    const auto& bundle = resourceManager.getStringResource(dataPath + "/kbv_bundle_unexpected_extension.xml");
    auto time = fhirtools::Timestamp::fromXsDateTime("2021-06-08T08:25:05+02:00");
    {
        EnvironmentVariableGuard onUnknownExtensionGuard{
            "ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION", "ignore"};
        EXPECT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, "X234567890", HttpStatus::OK, time));
    }
    {
        EnvironmentVariableGuard onUnknownExtensionGuard{
            "ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION", "report"};
        EXPECT_NO_FATAL_FAILURE(
            checkActivateTask(mServiceContext, task, bundle, "X234567890", HttpStatus::Accepted, time));
    }
    {
        EnvironmentVariableGuard onUnknownExtensionGuard{
            "ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION", "reject"};
        EXPECT_NO_FATAL_FAILURE(
            checkActivateTask(mServiceContext, task, bundle, "X234567890", HttpStatus::BadRequest, time));
    }
}


TEST_P(ActivateTaskValidationModeTest, genericValidation)
{
    using namespace std::string_literals;
    // clang-format off
    const auto message = "missing valueCoding.code in extension https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis"s;
    const auto fullDiagnostics =
    "Bundle.entry[0].resource{Composition}.extension[0].valueCoding: "
        "error: Expected exactly one system and one code sub-element "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Composition:rechtsgrundlage:valueCoding|1.0.2); "
    "Bundle.entry[0].resource{Composition}.extension[0].valueCoding: "
        "error: Expected exactly one system and one code sub-element "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Composition:rechtsgrundlage:valueCoding|1.0.2); "
    "Bundle.entry[0].resource{Composition}.extension[0].valueCoding.code: "
        "error: missing mandatory element "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Composition:rechtsgrundlage:valueCoding|1.0.2); "
    "Bundle.entry[0].resource{Composition}.extension[0].valueCoding: "
        "error: Expected exactly one system and one code sub-element "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis:valueCoding|1.0.3); "
    "Bundle.entry[0].resource{Composition}.extension[0].valueCoding: "
        "error: Expected exactly one system and one code sub-element "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis:valueCoding|1.0.3); "
    "Bundle.entry[0].resource{Composition}.extension[0].valueCoding.code: "
        "error: missing mandatory element "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis:valueCoding|1.0.3); "
    "Bundle: "
        "error: bdl-7: FullUrl must be unique in a bundle, or else entries with the same fullUrl must have different meta.versionId (except in history bundles) "
        "(from profile: http://hl7.org/fhir/StructureDefinition/Bundle|4.0.1); "
    "Bundle: "
        "error: bdl-7: FullUrl must be unique in a bundle, or else entries with the same fullUrl must have different meta.versionId (except in history bundles) "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.0.2); "s;
    // clang-format on
    EnvironmentVariableGuard onUnknownExtensionGuard{"ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION",
                                                     "report"};
    EnvironmentVariableGuard validationModeGuard{"ERP_SERVICE_GENERIC_VALIDATION_MODE",
                                                 std::string{magic_enum::enum_name(GetParam())}};
    auto& resourceManager = ResourceManager::instance();
    const auto& task = resourceManager.getStringResource(dataPath + "/task3.json");
    const auto& goodBundle = resourceManager.getStringResource(dataPath + "/kbv_bundle.xml");
    const auto& genericFailBundle = resourceManager.getStringResource(dataPath + "/kbv_bundle_duplicate_fullUrl.xml");
    const auto& invalidExtension = resourceManager.getStringResource(dataPath + "/kbv_bundle_unexpected_extension.xml");
    const auto& badBundle =
        resourceManager.getStringResource(dataPath + "/kbv_bundle_duplicate_fullUrl_missing_code.xml");
    auto time = fhirtools::Timestamp::fromXsDateTime("2021-06-08T08:25:05+02:00");

    EXPECT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, goodBundle, "X234567890", HttpStatus::OK, time));
    auto expectGeneric = (GetParam() == Configuration::GenericValidationMode::require_success)
                             ? (HttpStatus::BadRequest)
                             : (HttpStatus::OK);
    EXPECT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, genericFailBundle, "X234567890", expectGeneric, time));
    EXPECT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, invalidExtension, "X234567890", HttpStatus::Accepted, time));
    EXPECT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, badBundle, "X234567890", HttpStatus::BadRequest, time));
    try
    {
        ActivateTaskHandler handler({});
        ServerRequest request{serverRequest(task, badBundle, time)};
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        handler.handleRequest(sessionContext);
        ADD_FAILURE() << "handler.handleRequest should throw.";
    }
    catch (const ErpException& erpException)
    {
        ASSERT_EQ(erpException.status(), HttpStatus::BadRequest);
        EXPECT_EQ(erpException.what(), message);
        switch (GetParam())
        {
            using enum Configuration::GenericValidationMode;
            case Configuration::GenericValidationMode::disable:
                EXPECT_FALSE(erpException.diagnostics().has_value());
                break;
            case Configuration::GenericValidationMode::detail_only:
            case Configuration::GenericValidationMode::ignore_errors:
            case Configuration::GenericValidationMode::require_success:
                ASSERT_TRUE(erpException.diagnostics().has_value());
                EXPECT_EQ(*erpException.diagnostics(), fullDiagnostics);
        }
    }
}


INSTANTIATE_TEST_SUITE_P(AllModes, ActivateTaskValidationModeTest,
                         ::testing::ValuesIn(magic_enum::enum_values<Configuration::GenericValidationMode>()),
                         [](auto info) { return std::string{magic_enum::enum_name(info.param)};});


