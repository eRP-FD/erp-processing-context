/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/ErpRequirements.hxx"
#include "erp/service/task/ActivateTaskHandler.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"
#include "test/util/ResourceTemplates.hxx"

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
    struct ActivateTaskArgs {
        HttpStatus expectedStatus = HttpStatus::OK;
        std::optional<model::Timestamp> signingTime = std::nullopt;
        std::optional<model::Timestamp> expectedExpiry = std::nullopt;
        bool insertTask = false;
        std::optional<std::reference_wrapper<std::exception_ptr>> outExceptionPtr = std::nullopt;
    };

    void checkActivateTask(PcServiceContext& serviceContext, const std::string_view& taskJson,
                           const std::string_view& kbvBundleXml, const std::string_view& expectedKvnr,
                           ActivateTaskArgs args)
    {
        mockDatabase.reset();
        const auto& origTask = model::Task::fromJsonNoValidation(taskJson);

        ActivateTaskHandler handler({});
        ServerRequest request{serverRequest(taskJson, kbvBundleXml, args.signingTime)};
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{serviceContext, request, serverResponse, accessLog};
        if (args.insertTask)
        {
            createDatabase(mServiceContext.getHsmPool(), mServiceContext.getKeyDerivation());
            ASSERT_NE(mockDatabase, nullptr);
            mockDatabase->insertTask(origTask);
        }

        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        if (args.expectedStatus != HttpStatus::OK && args.expectedStatus != HttpStatus::Accepted)
        {
            auto callHandler = [&]{
                try {
                    handler.handleRequest(sessionContext);
                } catch (...) {
                    if (args.outExceptionPtr)
                    {
                        args.outExceptionPtr->get() = std::current_exception();
                    }
                    throw;
                }
            };
            EXPECT_ERP_EXCEPTION(callHandler(), args.expectedStatus);
            return;
        }

        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        ASSERT_EQ(serverResponse.getHeader().status(), args.expectedStatus);

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
        if (args.expectedExpiry.has_value())
        {
            EXPECT_EQ(*args.expectedExpiry, task->expiryDate());
        }
        EXPECT_FALSE(task->acceptDate().toXsDateTime().empty());
        EXPECT_TRUE(task->healthCarePrescriptionUuid().has_value());
        EXPECT_TRUE(task->patientConfirmationUuid().has_value());
        EXPECT_FALSE(task->receiptUuid().has_value());
    }
};

class ActivateTaskTestPkv : public ActivateTaskTest
{
protected:
    void SetUp() override
    {
        ActivateTaskTest::SetUp();
        auto currentVersion = model::ResourceVersion::current<model::ResourceVersion::FhirProfileBundleVersion>();
        if (currentVersion < model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)
        {
            GTEST_SKIP_(
                std::string("PKV not testable with ").append(model::ResourceVersion::v_str(currentVersion)).c_str());
        }
    }
};

TEST_F(ActivateTaskTest, ActivateTask)
{
    auto signingTime = model::Timestamp::fromXsDate("2021-06-08");
    const auto taskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml({.prescriptionId = taskId, .timestamp = signingTime});
    const auto taskJson = ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext,
                                              taskJson, kbvBundleXml, "X234567890", {.signingTime = signingTime}));
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkv)
{
    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50010);
    const char* const pkvKvnr = "X500000010";
    const auto timestamp = model::Timestamp::fromFhirDateTime("2021-06-08T13:44:53.012475+02:00");

    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = pkvTaskId, .timestamp = timestamp});
    const auto bundle =
        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId, .timestamp = timestamp, .kvnr = pkvKvnr});
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr, {.signingTime = timestamp}));
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkv209)
{
    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 50011);
    const char* const pkvKvnr = "X500000011";
    const auto timestamp = model::Timestamp::fromFhirDateTime("2021-06-08T13:44:53.012475+02:00");

    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = pkvTaskId, .timestamp = timestamp});
    const auto bundle =
        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId, .timestamp = timestamp, .kvnr = pkvKvnr});
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr, {.signingTime = timestamp}));
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkvInvalidCoverage200)
{
    A_22347_01.test("invalid coverage WF 200");
    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50010);
    const char* const pkvKvnr = "X500000010";
    const auto timestamp = model::Timestamp::fromFhirDateTime("2021-06-08T13:44:53.012475+02:00");
    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = pkvTaskId, .timestamp = timestamp});

    const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId,
                                                         .timestamp = timestamp,
                                                         .kvnr = pkvKvnr,
                                                         .coverageInsuranceType = "GKV",
                                                         .forceInsuranceType = "GKV",
                                                         .forceKvid10Ns = model::resource::naming_system::gkvKvid10});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr,
                                              {HttpStatus::BadRequest, timestamp, {}, false, exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(std::string(ex.what()), "Coverage \"PKV\" not set for flowtype 200/209")
            << ex.diagnostics().value_or("");
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkvInvalidCoverage209)
{
    A_22347_01.test("invalid coverage WF 209");
    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 50011);
    const char* const pkvKvnr = "X500000011";
    const auto timestamp = model::Timestamp::fromFhirDateTime("2021-06-08T13:44:53.012475+02:00");
    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = pkvTaskId, .timestamp = timestamp});

    const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId,
                                                         .timestamp = timestamp,
                                                         .kvnr = pkvKvnr,
                                                         .coverageInsuranceType = "GKV",
                                                         .forceInsuranceType = "GKV",
                                                         .forceKvid10Ns = model::resource::naming_system::gkvKvid10});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr,
                                              {HttpStatus::BadRequest, timestamp, {}, false, exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(std::string(ex.what()), "Coverage \"PKV\" not set for flowtype 200/209")
            << ex.diagnostics().value_or("");
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkvInvalidCoverage160)
{
    A_23443.test("invalid coverage WF 160");
    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 50010);
    const char* const pkvKvnr = "X500000010";
    const auto timestamp = model::Timestamp::fromFhirDateTime("2021-06-08T13:44:53.012475+02:00");
    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId, .timestamp = timestamp});

    const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId,
                                                         .timestamp = timestamp,
                                                         .kvnr = pkvKvnr,
                                                         .coverageInsuranceType = "PKV",
                                                         .forceInsuranceType = "PKV",
                                                         .forceKvid10Ns = model::resource::naming_system::pkvKvid10});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr,
                                              {HttpStatus::BadRequest, timestamp, {}, true, exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(std::string(ex.what()), "Coverage \"PKV\" not allowed for flowtype 160/169")
            << ex.diagnostics().value_or("");
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkvInvalidCoverage169)
{
    A_23443.test("invalid coverage WF 169");
    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

    const auto prescriptionId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisung, 50010);
    const char* const pkvKvnr = "X500000010";
    const auto timestamp = model::Timestamp::fromFhirDateTime("2021-06-08T13:44:53.012475+02:00");
    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId, .timestamp = timestamp});

    const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId,
                                                         .timestamp = timestamp,
                                                         .kvnr = pkvKvnr,
                                                         .coverageInsuranceType = "PKV",
                                                         .forceInsuranceType = "PKV",
                                                         .forceKvid10Ns = model::resource::naming_system::pkvKvid10});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr,
                                              {HttpStatus::BadRequest, timestamp, {}, true, exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(std::string(ex.what()), "Coverage \"PKV\" not allowed for flowtype 160/169")
            << ex.diagnostics().value_or("");
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

TEST_F(ActivateTaskTest, ActivateTaskBrokenSignature)
{
    const auto taskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto taskJson = ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext,
                                              taskJson,
                                              "",// empty signature
                                              "X234567890", {HttpStatus::BadRequest}));
}

TEST_F(ActivateTaskTest, AuthoredOnSignatureDateEquality)
{
    A_22487.test("Task aktivieren - Prüfregel Ausstellungsdatum");
    const auto* kvnr = "X234567890";
    const auto prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    auto task =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId});
    struct TestData
    {
        model::Timestamp signingTime;
        model::Timestamp authoredDate;
        HttpStatus expectedStatus;
    };
    const auto signingTime1 = model::Timestamp::fromXsDateTime("2022-06-25T22:33:00+00:00");
    const auto authoredTime1 = model::Timestamp::fromXsDate("2022-06-26");
    const auto signingTime2 = model::Timestamp::fromXsDateTime("2022-06-25T00:33:00+00:00");
    const auto authoredTime2 = model::Timestamp::fromXsDate("2022-06-25");
    const auto signingTime3 = model::Timestamp::fromXsDateTime("2022-06-26T12:33:00+00:00");
    const auto authoredTime3 = model::Timestamp::fromXsDate("2022-06-26");
    const auto signingTime4 = model::Timestamp::fromXsDateTime("2022-01-25T23:33:00+00:00");
    const auto authoredTime4 = model::Timestamp::fromXsDate("2022-01-26");
    const auto signingTime5 = model::Timestamp::fromXsDateTime("2022-01-25T22:33:00+00:00");
    const auto authoredTime5 = model::Timestamp::fromXsDate("2022-01-25");
    std::vector<TestData> testSet {
        {signingTime1, authoredTime1, HttpStatus::OK},
        {signingTime2, authoredTime1, HttpStatus::BadRequest},
        {signingTime3, authoredTime1, HttpStatus::OK},

        {signingTime1, authoredTime2, HttpStatus::BadRequest},
        {signingTime2, authoredTime2, HttpStatus::OK},
        {signingTime3, authoredTime2, HttpStatus::BadRequest},

        {signingTime1, authoredTime3, HttpStatus::OK},
        {signingTime2, authoredTime3, HttpStatus::BadRequest},
        {signingTime3, authoredTime3, HttpStatus::OK},

        {signingTime4, authoredTime4, HttpStatus::OK},
        {signingTime5, authoredTime4, HttpStatus::BadRequest},

        {signingTime5, authoredTime5, HttpStatus::OK},
        {signingTime4, authoredTime5, HttpStatus::BadRequest},
    };

    for (const auto& testData : testSet)
    {
        const auto bundle = ResourceTemplates::kbvBundleXml(
            {.prescriptionId = prescriptionId, .timestamp = testData.authoredDate, .kvnr = kvnr});
        ASSERT_NO_FATAL_FAILURE(
            checkActivateTask(mServiceContext, task, bundle, kvnr, {testData.expectedStatus, testData.signingTime}));
    }
    EnvironmentVariableGuard environmentVariableGuard3("ERP_SERVICE_TASK_ACTIVATE_AUTHORED_ON_MUST_EQUAL_SIGNING_DATE",
                                                       "false");
    for (const auto& testData : testSet)
    {
        const auto bundle = ResourceTemplates::kbvBundleXml(
            {.prescriptionId = prescriptionId, .timestamp = testData.authoredDate, .kvnr = kvnr});
        ASSERT_NO_FATAL_FAILURE(
            checkActivateTask(mServiceContext, task, bundle, kvnr, {.signingTime = testData.signingTime}));
    }
}


TEST_F(ActivateTaskTest, ThreeMonthExpiry)
{
    const auto* kvnr = "X234567890";
    const auto prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    const auto timestamp = model::Timestamp::fromXsDateTime("2022-08-31T18:40:00+00:00");
    const auto task = ResourceTemplates::taskJson({
        .taskType = ResourceTemplates::TaskType::Draft,
        .prescriptionId = prescriptionId,
        .timestamp = timestamp
    });
    auto bundle =
        ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId, .timestamp = timestamp, .kvnr = kvnr});

    auto expectedExpiryTime = model::Timestamp::fromGermanDate("2022-11-30");
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, kvnr,
                                              {.signingTime = timestamp, .expectedExpiry = expectedExpiryTime}));
}


TEST_F(ActivateTaskTest, Erp10633UnslicedExtension)
{
    EnvironmentVariableGuard onUnknownExtensionGuard{
            "ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION", "report"};
    const auto timestamp = model::Timestamp::fromXsDate("2022-07-25");
    const auto* kvnr = "Y229270213";
    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 429);
    auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId, .timestamp = timestamp, .kvnr = kvnr});
    // intent is only used for medication request and immediately after status, where we want to insert the extension
    auto extraStatusPos = bundle.find(R"(<intent value="order")");
    ASSERT_NE(extraStatusPos, std::string::npos);
    extraStatusPos = bundle.rfind("/>", extraStatusPos);
    ASSERT_NE(extraStatusPos, std::string::npos);
    bundle.replace(extraStatusPos, 2,
                  R"(><extension url="subStatus"><valueBoolean value="true"/></extension></status>)");
    auto taskJson = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId, .timestamp = timestamp, .kvnr = kvnr});
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, taskJson, bundle, kvnr,
                            {.expectedStatus = HttpStatus::Accepted, .signingTime = timestamp, .insertTask = true}));
}


class ActivateTaskValidationModeTest
    : public ActivateTaskTest
    , public ::testing::WithParamInterface<Configuration::GenericValidationMode>
{};

TEST_P(ActivateTaskValidationModeTest, OnUnexpectedKbvExtension)
{
    if (model::ResourceVersion::currentBundle() == model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)
    {
        GTEST_SKIP_("Skipping test because the kbv profile has closed slicing");
    }
    EnvironmentVariableGuard validationModeGuard{"ERP_SERVICE_GENERIC_VALIDATION_MODE",
                                                 std::string{magic_enum::enum_name(GetParam())}};
    EnvironmentVariableGuard validationModeGuardOld{"ERP_SERVICE_OLD_PROFILE_GENERIC_VALIDATION_MODE",
                                                 std::string{magic_enum::enum_name(GetParam())}};
    A_22927.test("E-Rezept-Fachdienst - Task aktivieren - Ausschluss unspezifizierter Extensions");
    auto time = model::Timestamp::fromXsDateTime("2021-06-08T08:25:05+02:00");
    const auto* kvnr = "X234567890";
    const auto prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    const std::string_view extraExtension =
        R"--(<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Illegal_extension">
          <valueCoding>
            <system value="https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"/>
            <code value="00"/>
          </valueCoding>
        </extension>)--";

    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId, .timestamp = time});
    const auto bundle = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = prescriptionId, .timestamp = time, .kvnr = kvnr, .metaExtension = extraExtension});

    {
        EnvironmentVariableGuard onUnknownExtensionGuard{
            "ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION", "ignore"};
        EXPECT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, kvnr, {.signingTime = time}));
    }
    {
        EnvironmentVariableGuard onUnknownExtensionGuard{
            "ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION", "report"};
        EXPECT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, kvnr, {HttpStatus::Accepted, time}));
    }
    {
        EnvironmentVariableGuard onUnknownExtensionGuard{
            "ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION", "reject"};
        EXPECT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, kvnr, {HttpStatus::BadRequest, time}));
    }
}


TEST_P(ActivateTaskValidationModeTest, genericValidation)
{
    using namespace std::string_literals;
    using enum Configuration::GenericValidationMode;
    bool usesNewProfile = model::ResourceVersion::currentBundle() ==
                          model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01;
    if (usesNewProfile && GetParam() != require_success)
    {
        // new profiles are always require_success, therefore we cannot test any other modes
        GTEST_SKIP();
    }
    const auto message =
        usesNewProfile
            ? "FHIR-Validation error"s
            : "missing valueCoding.code in extension https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis"s;

    auto time = model::Timestamp::fromXsDateTime("2021-06-08T08:25:05+02:00");
    const auto* kvnr = "X234567890";
    const auto prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    const std::string_view extraExtension =
        R"--(<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Illegal_extension">
          <valueCoding>
            <system value="https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"/>
            <code value="00"/>
          </valueCoding>
        </extension>)--";
    EnvironmentVariableGuard onUnknownExtensionGuard{
        ConfigurationKey::SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION, "report"};
    EnvironmentVariableGuard validationModeGuardOld{
        ConfigurationKey::SERVICE_OLD_PROFILE_GENERIC_VALIDATION_MODE, std::string{magic_enum::enum_name(GetParam())}};
    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId, .timestamp = time});
    const auto invalidExtensionBundle = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = prescriptionId, .timestamp = time, .kvnr = kvnr, .metaExtension = extraExtension});

    auto genericFailBundleDuplicateUrl = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = prescriptionId, .timestamp = time, .kvnr = kvnr});
    // for generic fail, duplicate the fullUrl for two entries
    std::string::size_type fullUrlEndPos = 0u;
    for (int i = 0; i < 2; ++i)
    {
        auto fullUrlStartPos = genericFailBundleDuplicateUrl.find("<fullUrl value=", fullUrlEndPos);
        fullUrlEndPos = genericFailBundleDuplicateUrl.find("/>", fullUrlStartPos);
        ASSERT_NE(fullUrlStartPos, std::string::npos);
        ASSERT_NE(fullUrlEndPos, std::string::npos);
        ASSERT_GT(fullUrlEndPos, fullUrlStartPos);
        fullUrlEndPos += 2;
        genericFailBundleDuplicateUrl.replace(fullUrlStartPos, fullUrlEndPos - fullUrlStartPos, "<fullUrl value=\"http://erp-test.net/duplicateUrl\"/>");
    }
    auto badBundle = genericFailBundleDuplicateUrl;
    {
        const auto codeStartPos = genericFailBundleDuplicateUrl.find("<code");
        const auto codeEndPos = genericFailBundleDuplicateUrl.find('>', codeStartPos);
        ASSERT_NE(codeStartPos, std::string::npos);
        ASSERT_NE(codeEndPos, std::string::npos);
        ASSERT_GT(codeEndPos, codeStartPos);
        badBundle.erase(codeStartPos, codeEndPos - codeStartPos + 1);
    }
    auto goodBundle = ResourceTemplates::kbvBundleXml({.timestamp = time, .kvnr = kvnr});

    bool requireGenericSuccess = GetParam() == Configuration::GenericValidationMode::require_success;
    EXPECT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, goodBundle, kvnr, {.signingTime = time}));
    auto expectGeneric = requireGenericSuccess ? (HttpStatus::BadRequest) : (HttpStatus::OK);
    EXPECT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, genericFailBundleDuplicateUrl, kvnr, {expectGeneric, time}));
    EXPECT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, invalidExtensionBundle, kvnr, {HttpStatus::Accepted, time}));

    // this can only fail if the generica validator requires success for new profile versions
    if (model::ResourceVersion::currentBundle() == model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01 ||
        requireGenericSuccess)
    {
        EXPECT_NO_FATAL_FAILURE(
            checkActivateTask(mServiceContext, task, badBundle, kvnr, {HttpStatus::BadRequest, time}));
    }
    try
    {
        mockDatabase.reset();
        ActivateTaskHandler handler({});
        ServerRequest request{serverRequest(task, badBundle, time)};
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        handler.handleRequest(sessionContext);
        // for new profiles, there is no xsd validation, and if generic validation is disabled,
        // nothing can throw
        if (model::ResourceVersion::currentBundle() == model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01 ||
            requireGenericSuccess)
        {
            ADD_FAILURE() << "handler.handleRequest should throw.";
        }
    }
    catch (const ErpException& erpException)
    {
        ASSERT_EQ(erpException.status(), HttpStatus::BadRequest);
        EXPECT_EQ(erpException.what(), message);
        switch (GetParam())
        {

            case Configuration::GenericValidationMode::disable:
                EXPECT_FALSE(erpException.diagnostics().has_value());
                break;
            case Configuration::GenericValidationMode::detail_only:
            case Configuration::GenericValidationMode::ignore_errors:
            case Configuration::GenericValidationMode::require_success: {
                ASSERT_TRUE(erpException.diagnostics().has_value());
                std::vector<std::string> diagnosticsMessages = String::split(erpException.diagnostics().value(), ";");
                // as the errors may vary between profile versions, be happy if there are "enough" errors
                // specific errors can be tested separately by the validator tests
                EXPECT_GT(diagnosticsMessages.size(), 5);
            }
        }
    }
}


INSTANTIATE_TEST_SUITE_P(AllModes, ActivateTaskValidationModeTest,
                         ::testing::ValuesIn(magic_enum::enum_values<Configuration::GenericValidationMode>()),
                         [](auto info) { return std::string{magic_enum::enum_name(info.param)};});


TEST_F(ActivateTaskTest, ERP_12708_kbv_bundle_RepeatedErrorMessages)
{
    static const std::string expectedDiagnostic{
        R"(Bundle.entry[1].resource{Patient}.identifier[0]: error: )"
        R"(element doesn't match any slice in closed slicing)"
        R"( (from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Patient|1.1.0); )"};
    EnvironmentVariableGuard validFromGuard{"ERP_FHIR_PROFILE_VALID_FROM", "2022-11-01T00:00:00+01:00"};
    ResourceManager& resourceManager = ResourceManager::instance();
    const auto& kbvBundle =
        resourceManager.getStringResource("test/EndpointHandlerTest/ERP-12708-kbv_bundle-RepeatedErrorMessages.xml");
    std::string_view kvnr{"X123456789"};
    auto task = ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .kvnr = kvnr});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, kbvBundle, kvnr,
                          {.expectedStatus = HttpStatus::BadRequest, .outExceptionPtr = exception}));
    try {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        ASSERT_TRUE(ex.diagnostics().has_value());
        EXPECT_EQ(ex.diagnostics().value(), expectedDiagnostic);
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

TEST_F(ActivateTaskTest, ERP12860_WrongProfile)
{
    std::string_view kvnr{"X123456789"};
    auto kbvBundle = ResourceTemplates::kbvBundleXml(
        {.timestamp = model::Timestamp::now(), .kvnr = kvnr, .kbvVersion = model::ResourceVersion::KbvItaErp::v1_1_0});
    kbvBundle = String::replaceAll(kbvBundle, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle",
                                   "https://sample.de/StructureDefinition/KBV_PR_ERP_Bundle");
    auto task = ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .kvnr = kvnr});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, kbvBundle, kvnr,
                          {.expectedStatus = HttpStatus::BadRequest, .outExceptionPtr = exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        ASSERT_TRUE(ex.diagnostics().has_value());
        // see ERP-13187:
        EXPECT_EQ(ex.diagnostics().value(), "Unable to determine profile type from name: "
                                            "https://sample.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0");
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

class ActivateTaskTestPkvP : public ActivateTaskTest, public ::testing::WithParamInterface<model::PrescriptionType>
{
};

TEST_P(ActivateTaskTestPkvP, RejectOldFhirForWf20x)
{
    if (! model::ResourceVersion::supportedBundles().contains(
            model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01))
    {
        GTEST_SKIP_("This test is only relevant for 2022 profiles");
    }
    std::string_view kvnr{"X123456789"};
    auto prescriptionId = model::PrescriptionId::fromDatabaseId(GetParam(), 333);
    auto kbvBundle = ResourceTemplates::kbvBundlePkvXml(ResourceTemplates::KbvBundlePkvOptions(
        prescriptionId, model::Kvnr(kvnr), model::ResourceVersion::KbvItaErp::v1_0_2));
    auto task =
        ResourceTemplates::taskJson({.gematikVersion = model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1,
                                     .taskType = ResourceTemplates::TaskType::Draft,
                                     .prescriptionId = prescriptionId,
                                     .kvnr = kvnr});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, kbvBundle, kvnr,
                                              {.expectedStatus = HttpStatus::BadRequest,
                                               .signingTime = model::Timestamp::fromXsDate("2021-06-08"),
                                               .insertTask = true,
                                               .outExceptionPtr = exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(std::string(ex.what()), "unsupported fhir version for PKV workflow 200/209: "
                             "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.0.2");
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

INSTANTIATE_TEST_SUITE_P(prefix, ActivateTaskTestPkvP,
                         ::testing::Values(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
                                           model::PrescriptionType::direkteZuweisungPkv));
