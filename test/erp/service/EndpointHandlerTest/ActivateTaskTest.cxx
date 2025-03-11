/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/DatabaseFrontend.hxx"
#include "erp/service/task/ActivateTaskHandler.hxx"
#include "shared/ErpRequirements.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

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
        std::optional<model::Timestamp> expectedAcceptDate = std::nullopt;
        bool insertTask = false;
        std::optional<std::reference_wrapper<std::exception_ptr>> outExceptionPtr = std::nullopt;
        std::optional<std::reference_wrapper<Header>> outHeader = std::nullopt;
        std::optional<std::string_view> flowTypeDisplayName = std::nullopt;
    };

    void checkActivateTask(PcServiceContext& serviceContext, const std::string_view& taskJson,
                           const std::string_view& kbvBundleXml, const std::string_view& expectedKvnr,
                           ActivateTaskArgs args)
    {
        mockDatabase.reset();
        const auto& origTask = model::Task::fromJsonNoValidation(taskJson);

        ActivateTaskHandler handler({});
        ServerRequest request{serverRequest(taskJson, kbvBundleXml, args.signingTime)};
        request.setAccessToken(mJwtBuilder->makeJwtArzt());
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
        if (args.expectedStatus != HttpStatus::OK && args.expectedStatus != HttpStatus::Accepted &&
            args.expectedStatus != HttpStatus::OkWarnInvalidAnr)
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
        if (args.outHeader)
        {
            args.outHeader->get() = serverResponse.getHeader();
        }

        std::optional<model::Task> task;
        ASSERT_NO_THROW(task = model::Task::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator()));
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
        if (args.expectedAcceptDate.has_value())
        {
            EXPECT_EQ(*args.expectedAcceptDate, task->acceptDate());
        }
        EXPECT_TRUE(task->healthCarePrescriptionUuid().has_value());
        EXPECT_TRUE(task->patientConfirmationUuid().has_value());
        EXPECT_FALSE(task->receiptUuid().has_value());

        if (args.flowTypeDisplayName.has_value())
        {
            auto ext = task->getExtension(model::resource::structure_definition::prescriptionType);
            ASSERT_TRUE(ext.has_value());
            auto flowTypeDisplay = ext->valueCodingDisplay();
            ASSERT_TRUE(flowTypeDisplay.has_value());
            EXPECT_EQ(flowTypeDisplay, args.flowTypeDisplayName.value());
        }

        using namespace model::resource;
        using fn = std::optional<std::string_view> (*)(const rapidjson::Value& object, const rapidjson::Pointer& key);
        static constexpr auto getOptionalStringValue =
        static_cast<fn>(&model::NumberAsStringParserDocument::getOptionalStringValue);
        auto taskDoc = model::NumberAsStringParserDocument::fromJson(task->serializeToJsonString());
        static const rapidjson::Pointer performerTypeCodingPtr{
            ElementName::path(ElementName{"performerType"}, 0, elements::coding, 0)};
        auto* performerTypeCoding = performerTypeCodingPtr.Get(taskDoc);
        ASSERT_NE(performerTypeCoding, nullptr);
        static const rapidjson::Pointer systemPtr{ElementName::path(elements::system)};
        static const rapidjson::Pointer codePtr{ElementName::path(elements::code)};
        static const rapidjson::Pointer displayPtr{ElementName::path(elements::display)};
        auto performerTypeCodingSystem = getOptionalStringValue(*performerTypeCoding, systemPtr);
        auto performerTypeCodingCode = getOptionalStringValue(*performerTypeCoding, codePtr);
        auto performerTypeCodingDisplay = getOptionalStringValue(*performerTypeCoding, displayPtr);
        ASSERT_TRUE(performerTypeCodingSystem.has_value());
        ASSERT_EQ(std::string{*performerTypeCodingSystem},
                  "https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_OrganizationType");
        ASSERT_TRUE(performerTypeCodingCode.has_value());
        ASSERT_TRUE(performerTypeCodingDisplay.has_value());
        if (isDiga(task->prescriptionId().type()))
        {
            ASSERT_EQ(std::string{*performerTypeCodingCode}, "urn:oid:1.2.276.0.76.4.59");
            ASSERT_EQ(std::string{*performerTypeCodingDisplay}, "Kostenträger");
        }
        else
        {
            ASSERT_EQ(std::string{*performerTypeCodingCode}, "urn:oid:1.2.276.0.76.4.54");
            ASSERT_EQ(std::string{*performerTypeCodingDisplay}, "Öffentliche Apotheke");
        }
    }
};

class ActivateTaskTestPkv : public ActivateTaskTest
{
};

TEST_F(ActivateTaskTest, ActivateTask)
{
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml({.prescriptionId = taskId, .authoredOn = signingTime});
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, taskJson, kbvBundleXml, "X234567891", {.signingTime = signingTime}));
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkv)
{
    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50010);
    const char* const pkvKvnr = "X500000017";
    const auto authoredOn = model::Timestamp::now();

    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = pkvTaskId, .timestamp = authoredOn});
    const auto bundle =
        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId, .authoredOn = authoredOn, .kvnr = pkvKvnr});
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr, {.signingTime = authoredOn}));
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkv209)
{
    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 50011);
    const char* const pkvKvnr = "X500000029";
    const auto authoredOn = model::Timestamp::now();

    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = pkvTaskId, .timestamp = authoredOn});
    const auto bundle =
        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId, .authoredOn = authoredOn, .kvnr = pkvKvnr});
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr, {.signingTime = authoredOn}));
}

TEST_F(ActivateTaskTest, ActivateTaskDiga)
{
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6001);
    const auto kbvBundleXml = ResourceTemplates::evdgaBundleXml(
        {.prescriptionId = taskId.toString(), .kvnr = "X234567891", .authoredOn = signingTime.toGermanDate()});
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, taskJson, kbvBundleXml, "X234567891", {.signingTime = signingTime}));
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkvInvalidCoverage200)
{
    A_22347_01.test("invalid coverage WF 200");

    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50010);
    const char* const pkvKvnr = "X500000017";
    const auto authoredOn = model::Timestamp::now();
    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = pkvTaskId, .timestamp = authoredOn});

    const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId,
                                                         .authoredOn = authoredOn,
                                                         .kvnr = pkvKvnr,
                                                         .coverageInsuranceType = "GKV",
                                                         .forceInsuranceType = "GKV",
                                                         .forceKvid10Ns = model::resource::naming_system::gkvKvid10});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr,
                                              {.expectedStatus = HttpStatus::BadRequest,
                                               .signingTime = authoredOn,
                                               .insertTask = false,
                                               .outExceptionPtr = exception}));
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

    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 50011);
    const char* const pkvKvnr = "X500000029";
    const auto authoredOn = model::Timestamp::now();
    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = pkvTaskId, .timestamp = authoredOn});

    const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId,
                                                         .authoredOn = authoredOn,
                                                         .kvnr = pkvKvnr,
                                                         .coverageInsuranceType = "GKV",
                                                         .forceInsuranceType = "GKV",
                                                         .forceKvid10Ns = model::resource::naming_system::gkvKvid10});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr,
                                              {.expectedStatus = HttpStatus::BadRequest,
                                               .signingTime = authoredOn,
                                               .insertTask = false,
                                               .outExceptionPtr = exception}));
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
    A_23443_01.test("invalid coverage WF 160");

    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 50010);
    const char* const pkvKvnr = "X500000017";
    const auto authoredOn = model::Timestamp::now();
    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId, .timestamp = authoredOn});

    const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId,
                                                         .authoredOn = authoredOn,
                                                         .kvnr = pkvKvnr,
                                                         .coverageInsuranceType = "PKV",
                                                         .forceInsuranceType = "PKV",
                                                         .forceKvid10Ns = model::resource::naming_system::pkvKvid10});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr,
                                              {.expectedStatus = HttpStatus::BadRequest,
                                               .signingTime = authoredOn,
                                               .insertTask = true,
                                               .outExceptionPtr = exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(std::string(ex.what()), "Coverage \"PKV\" not allowed for flowtype 160/162/169")
            << ex.diagnostics().value_or("");
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkvInvalidCoverage169)
{
    A_23443_01.test("invalid coverage WF 169");

    const auto prescriptionId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisung, 50010);
    const char* const pkvKvnr = "X500000017";
    const auto authoredOn = model::Timestamp::now();
    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId, .timestamp = authoredOn});

    const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId,
                                                         .authoredOn = authoredOn,
                                                         .kvnr = pkvKvnr,
                                                         .coverageInsuranceType = "PKV",
                                                         .forceInsuranceType = "PKV",
                                                         .forceKvid10Ns = model::resource::naming_system::pkvKvid10});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr,
                                              {.expectedStatus = HttpStatus::BadRequest,
                                               .signingTime = authoredOn,
                                               .insertTask = true,
                                               .outExceptionPtr = exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(std::string(ex.what()), "Coverage \"PKV\" not allowed for flowtype 160/162/169")
            << ex.diagnostics().value_or("");
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkvInvalidCoverage162)
{
    A_23443_01.test("invalid coverage WF 162");

    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 50010);
    const char* const pkvKvnr = "X500000017";
    const auto authoredOn = model::Timestamp::now();
    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId, .timestamp = authoredOn});

    const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId,
                                                         .authoredOn = authoredOn,
                                                         .kvnr = pkvKvnr,
                                                         .coverageInsuranceType = "PKV",
                                                         .forceInsuranceType = "PKV",
                                                         .forceKvid10Ns = model::resource::naming_system::pkvKvid10});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr,
                                              {.expectedStatus = HttpStatus::BadRequest,
                                               .signingTime = authoredOn,
                                               .insertTask = true,
                                               .outExceptionPtr = exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        // Its not getting to the point where the coverage is checked, it fails way earlier...
        EXPECT_EQ(std::string(ex.what()),
                  "Für diesen Workflowtypen sind nur Verordnungen für Digitale Gesundheitsanwendungen zulässig")
            << ex.diagnostics().value_or("");
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

TEST_F(ActivateTaskTest, ActivateTaskInvalidPatientCodingCode)
{
    A_23936.test("Test invalid Patient.Coding.Code");
    const auto* kvnr = "123456";
    const auto prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    auto signingTime = model::Timestamp::now();
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = prescriptionId,
         .authoredOn = signingTime,
         .kvnr = kvnr,
         .coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis",
         .coverageInsuranceType = "SEL",
         .forceInsuranceType = "kvk",
         .forceKvid10Ns = "http://fhir.de/sid/gkv/kvk-versichertennummer",
         .patientIdentifierSystem = "https://fhir.kbv.de/CodeSystem/KBV_CS_Base_identifier_type"});
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = prescriptionId});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, taskJson, kbvBundleXml, kvnr,
                                              {.expectedStatus = HttpStatus::BadRequest,
                                               .signingTime = signingTime,
                                               .expectedExpiry = std::nullopt,
                                               .outExceptionPtr = exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(std::string(ex.what()),
                  "Als Identifier für den Patienten muss eine Versichertennummer angegeben werden.")
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
                                              "X234567891", {HttpStatus::BadRequest}));
}

TEST_F(ActivateTaskTest, AuthoredOnSignatureDateEquality)
{
    A_22487.test("Task aktivieren - Prüfregel Ausstellungsdatum");
    const auto* kvnr = "X234567891";
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
    const auto authoredTime1 = model::Timestamp::fromXsDate("2022-06-26", model::Timestamp::UTCTimezone);
    const auto signingTime2 = model::Timestamp::fromXsDateTime("2022-06-25T00:33:00+00:00");
    const auto authoredTime2 = model::Timestamp::fromXsDate("2022-06-25", model::Timestamp::UTCTimezone);
    const auto signingTime3 = model::Timestamp::fromXsDateTime("2022-06-26T12:33:00+00:00");
    const auto authoredTime3 = model::Timestamp::fromXsDate("2022-06-26", model::Timestamp::UTCTimezone);
    const auto signingTime4 = model::Timestamp::fromXsDateTime("2022-01-25T23:33:00+00:00");
    const auto authoredTime4 = model::Timestamp::fromXsDate("2022-01-26", model::Timestamp::UTCTimezone);
    const auto signingTime5 = model::Timestamp::fromXsDateTime("2022-01-25T22:33:00+00:00");
    const auto authoredTime5 = model::Timestamp::fromXsDate("2022-01-25", model::Timestamp::UTCTimezone);
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
        const auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(testData.authoredDate);
        const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId,
                                                             .authoredOn = testData.authoredDate,
                                                             .kvnr = kvnr,
                                                             .kbvVersion = kbvVersion});
        ASSERT_NO_FATAL_FAILURE(
            checkActivateTask(mServiceContext, task, bundle, kvnr, {testData.expectedStatus, testData.signingTime}));
    }
}


TEST_F(ActivateTaskTest, ThreeMonthExpiry)
{
    const auto* kvnr = "X234567891";
    const auto prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    const auto authoredOn = model::Timestamp::fromXsDateTime("2022-08-31T18:40:00+00:00");
    const auto task = ResourceTemplates::taskJson({
        .taskType = ResourceTemplates::TaskType::Draft,
        .prescriptionId = prescriptionId,
        .timestamp = authoredOn
    });
    const auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    auto bundle = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = prescriptionId, .authoredOn = authoredOn, .kvnr = kvnr, .kbvVersion = kbvVersion});

    auto expectedExpiryTime = model::Timestamp::fromGermanDate("2022-11-30");
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, kvnr,
                                              {.signingTime = authoredOn, .expectedExpiry = expectedExpiryTime}));
}


TEST_F(ActivateTaskTest, Erp10633UnslicedExtension)
{
    EnvironmentVariableGuard onUnknownExtensionGuard{
            "ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION", "report"};
    const auto timestamp = model::Timestamp::fromXsDate("2022-07-25", model::Timestamp::UTCTimezone);
    const auto* kvnr = "Y229270213";
    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 429);
    const auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(timestamp);
    auto bundle = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = prescriptionId, .authoredOn = timestamp, .kvnr = kvnr, .kbvVersion = kbvVersion});
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


TEST_F(ActivateTaskTest, authoredOnReference)
{
    auto now = std::chrono::system_clock::now();
    auto today{date::floor<date::days>(now)};
    auto yesterday{today - date::days{1}};

    testutils::ShiftFhirResourceViewsGuard viewGuard{"KBV_2023-07-01_2025-Q2", fhirtools::Date{today, fhirtools::Date::Precision::day}};

    using namespace std::chrono_literals;
    const auto* kvnr = "X234567891";
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto taskJson = ResourceTemplates::taskJson({.gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_4,
                                                       .taskType = ResourceTemplates::TaskType::Ready,
                                                       .prescriptionId = taskId});

    // authoredOn is in DARREICHUNGSFORM 1.14 period (yesterday)
    {
        const auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
            .prescriptionId = taskId,
            .authoredOn = model::Timestamp{yesterday},
            .medicationOptions =
                {
                    .version = ResourceTemplates::Versions::KBV_ERP_current(),
                    .darreichungsform = "LYE",
                },
        });
        ASSERT_NO_FATAL_FAILURE(
            checkActivateTask(mServiceContext, taskJson, kbvBundleXml, kvnr,
                              {.expectedStatus = HttpStatus::BadRequest, .signingTime = model::Timestamp{yesterday}}));
    }

    // authoredOn is in DARREICHUNGSFORM 1.15 period (today)
    {
        const auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
            .prescriptionId = taskId,
            .authoredOn = model::Timestamp{today},
            .medicationOptions =
                {
                    .version = ResourceTemplates::Versions::KBV_ERP_current(),
                    .darreichungsform = "LYE",
                },
        });
        ASSERT_NO_FATAL_FAILURE(
            checkActivateTask(mServiceContext, taskJson, kbvBundleXml, kvnr, {.signingTime = model::Timestamp{today}}));
    }
}

TEST_F(ActivateTaskTest, ERP_12708_kbv_bundle_RepeatedErrorMessages)
{
    static const std::string expectedDiagnostic{
        R"(Bundle.entry[1].resource{Patient}.identifier[0]: error: )"
        R"(element doesn't match any slice in closed slicing)"
        R"( (from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Patient|1.1.0); )"};
    ResourceManager& resourceManager = ResourceManager::instance();
    const auto& kbvBundle =
        resourceManager.getStringResource("test/EndpointHandlerTest/ERP-12708-kbv_bundle-RepeatedErrorMessages.xml");
    std::string_view kvnr{"X123456788"};
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
    std::string_view kvnr{"X123456788"};
    auto kbvBundle = ResourceTemplates::kbvBundleXml({.authoredOn = model::Timestamp::now(), .kvnr = kvnr});
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
        EXPECT_EQ(ex.diagnostics().value(),
                  "invalid profile https://sample.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0"
                  " must be one of: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0");
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

TEST_F(ActivateTaskTest, ERP15117_begrenzungDateEnd)
{
    std::string_view kvnr{"X123456788"};
    auto task = ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .kvnr = kvnr});
    auto kbvBundle = ResourceTemplates::kbvBundleMvoXml({.redeemPeriodEnd = "2021-01-02T23:59:59.999+01:00"});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, kbvBundle, kvnr,
                          {.expectedStatus = HttpStatus::BadRequest, .outExceptionPtr = exception}));
    ASSERT_TRUE(exception);
    EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(
        std::rethrow_exception(exception),
        "Bundle.entry[1].resource{MedicationRequest}.extension[3].extension[2].valuePeriod: error: "
        "-erp-begrenzungDateEnd: Begrenzung der Datumsangabe auf 10 Zeichen JJJJ-MM-TT (from profile: "
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription:Zeitraum:valuePeriod|1.1.0); "
        "Bundle.entry[1].resource{MedicationRequest}.extension[3].extension[2].valuePeriod: error: "
        "-erp-begrenzungDateEnd: Begrenzung der Datumsangabe auf 10 Zeichen JJJJ-MM-TT (from profile: "
        "https://fhir.kbv.de/StructureDefinition/"
        "KBV_PR_ERP_Prescription:Mehrfachverordnung:Zeitraum:valuePeriod|1.1.0); "
        "Bundle.entry[1].resource{MedicationRequest}.extension[3].extension[2].valuePeriod: error: per-1: If present, "
        "start SHALL have a lower value than end (from profile: "
        "http://hl7.org/fhir/StructureDefinition/Period|4.0.1); ");
}

TEST_F(ActivateTaskTest, ERP15117_begrenzungDateStart)
{
    std::string_view kvnr{"X123456788"};
    auto task = ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .kvnr = kvnr});
    auto kbvBundle = ResourceTemplates::kbvBundleMvoXml({.redeemPeriodStart = "2021-01-02T23:59:59.999+01:00"});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, kbvBundle, kvnr,
                          {.expectedStatus = HttpStatus::BadRequest, .outExceptionPtr = exception}));
    ASSERT_TRUE(exception);
    EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(
        std::rethrow_exception(exception),
        "Bundle.entry[1].resource{MedicationRequest}.extension[3].extension[2].valuePeriod: error: "
        "-erp-begrenzungDateStart: Begrenzung der Datumsangabe auf 10 Zeichen JJJJ-MM-TT (from profile: "
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription:Zeitraum:valuePeriod|1.1.0); "
        "Bundle.entry[1].resource{MedicationRequest}.extension[3].extension[2].valuePeriod: error: "
        "-erp-begrenzungDateStart: Begrenzung der Datumsangabe auf 10 Zeichen JJJJ-MM-TT (from profile: "
        "https://fhir.kbv.de/StructureDefinition/"
        "KBV_PR_ERP_Prescription:Mehrfachverordnung:Zeitraum:valuePeriod|1.1.0); "
        "Bundle.entry[1].resource{MedicationRequest}.extension[3].extension[2].valuePeriod: error: per-1: If present, "
        "start SHALL have a lower value than end (from profile: "
        "http://hl7.org/fhir/StructureDefinition/Period|4.0.1); ");
}

TEST_F(ActivateTaskTest, ERP20812_acceptUpperCase_ID)
{
    std::string_view kvnr{"X234567891"};
    auto task = ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .kvnr = kvnr});
    auto mvoStart = model::Timestamp::now().toGermanDate();
    //auto signingTime = model::Timestamp::now();
    auto kbvBundle = ResourceTemplates::kbvBundleMvoXml(
        {.redeemPeriodStart = mvoStart, .redeemPeriodEnd = mvoStart, .mvoId = "urn:uuid:3C624210-0211-42F0-AFF2-E2814980E154"});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, kbvBundle, kvnr,
                          {.expectedStatus = HttpStatus::OK, .outExceptionPtr = exception}));
    ASSERT_FALSE(exception);
}

TEST_F(ActivateTaskTest, failInvalidKvnr)
{
    A_23890.test("Test invalid KVNR");
    const model::Kvnr invalidKvnr{"B123457890"};
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto kbvBundleXml =
        ResourceTemplates::kbvBundleXml({.prescriptionId = taskId, .authoredOn = signingTime, .kvnr = invalidKvnr.id()});
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});

    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, invalidKvnr.id(),
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(
        std::rethrow_exception(exception), HttpStatus::BadRequest,
        "Ungültige Versichertennummer (KVNR): Die übergebene Versichertennummer des Patienten entspricht nicht den "
        "Prüfziffer-Validierungsregeln.");
}

TEST_F(ActivateTaskTest, failInvalidIK)
{
    A_23888.test("Test invalid IK");
    const model::Kvnr kvnr{"B123457897"};
    const model::Iknr iknr{"109911214"};
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = taskId, .authoredOn = signingTime, .kvnr = kvnr.id(), .iknr = iknr});
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});

    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, kvnr.id(),
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(
        std::rethrow_exception(exception), HttpStatus::BadRequest,
        "Ungültiges Institutionskennzeichen (IKNR): Das übergebene Institutionskennzeichen im Versicherungsstatus "
        "entspricht nicht den Prüfziffer-Validierungsregeln.");
}

TEST_F(ActivateTaskTest, validWithoutIK)
{
    A_23888.test("Test valid case without IK");
    const model::Kvnr kvnr{"B123457897"};
    const model::Iknr iknr{""};
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
        .prescriptionId = taskId, .authoredOn = signingTime, .kvnr = kvnr.id(),
        .coverageInsuranceType = "SEL", .iknr = iknr });
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});

    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, kvnr.id(), {.signingTime = signingTime}));
}

TEST_F(ActivateTaskTest, failInvalidAlternativeIK)
{
    A_24030.test("Invalid IKNR in coverage extension");
    const model::Kvnr kvnr{"B123457897"};
    const model::Iknr iknr{"109911214"};
    const auto extensionCoverage =
        R"(<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Alternative_IK"><valueIdentifier><system value=")" +
        std::string{iknr.namingSystem()} + R"(" /><value value=")" + iknr.id() +
        R"(" /></valueIdentifier></extension>)";
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = taskId,
         .authoredOn = signingTime,
         .kvnr = kvnr.id(),
         .coverageInsuranceSystem = "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Payor_Type_KBV",
         .coverageInsuranceType = "UK",
         .coveragePayorExtension = extensionCoverage});
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});

    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, kvnr.id(),
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(
        std::rethrow_exception(exception), HttpStatus::BadRequest,
        "Ungültiges Institutionskennzeichen (IKNR): Das übergebene Institutionskennzeichen des Kostenträgers "
        "entspricht nicht den Prüfziffer-Validierungsregeln.");
}

TEST_F(ActivateTaskTest, failInvalidANR)
{
    A_24032.test("Test invalid ANR - fail");
    const model::Kvnr kvnr{"B123457897"};
    EnvironmentVariableGuard failLanrConfig{ConfigurationKey::SERVICE_TASK_ACTIVATE_ANR_VALIDATION_MODE, "error"};
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    for (const auto type : {model::Lanr::Type::lanr, model::Lanr::Type::zanr})
    {
        const model::Lanr lanr{"444444300", type};
        const auto kbvBundleXml = ResourceTemplates::kbvBundleXml(
            {.prescriptionId = taskId, .authoredOn = signingTime, .kvnr = kvnr.id(), .lanr = lanr});
        const auto taskJson =
            ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});

        std::exception_ptr exception;
        ASSERT_NO_FATAL_FAILURE(checkActivateTask(
            mServiceContext, taskJson, kbvBundleXml, kvnr.id(),
            {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
        EXPECT_ERP_EXCEPTION_WITH_MESSAGE(std::rethrow_exception(exception), HttpStatus::BadRequest,
                                          "Ungültige Arztnummer (LANR oder ZANR): Die übergebene Arztnummer entspricht "
                                          "nicht den Prüfziffer-Validierungsregeln.");
    }
}

TEST_F(ActivateTaskTest, warnInvalidANR)
{
    A_24033.test("Test invalid ANR - warning");
    const model::Kvnr kvnr{"B123457897"};
    EnvironmentVariableGuard failLanrConfig{ConfigurationKey::SERVICE_TASK_ACTIVATE_ANR_VALIDATION_MODE, "warning"};
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    for (const auto type : {model::Lanr::Type::lanr, model::Lanr::Type::zanr})
    {
        const model::Lanr lanr{"444444300", type};

        const auto kbvBundleXml = ResourceTemplates::kbvBundleXml(
            {.prescriptionId = taskId, .authoredOn = signingTime, .kvnr = kvnr.id(), .lanr = lanr});
        const auto taskJson =
            ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
        Header responseHeader;
        ASSERT_NO_FATAL_FAILURE(checkActivateTask(
            mServiceContext, taskJson, kbvBundleXml, kvnr.id(),
            {.expectedStatus = HttpStatus::OkWarnInvalidAnr, .signingTime = signingTime, .outHeader = responseHeader}));
        ASSERT_TRUE(responseHeader.header(Header::Warning).has_value());
        ASSERT_EQ(responseHeader.header(Header::Warning).value(),
                  "252 erp-server \"Ungültige Arztnummer (LANR oder ZANR): Die übergebene Arztnummer entspricht nicht "
                  "den Prüfziffer-Validierungsregeln.\"");
    }
}

TEST_F(ActivateTaskTest, failInvalidPZNChecksum)
{
    A_23892.test("pzn checksum must be correct");
    const model::Kvnr kvnr{"B123457897"};
    const model::Pzn pzn{"27580891"};
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
        .prescriptionId = taskId,
        .authoredOn = signingTime,
        .kvnr = kvnr.id(),
        .medicationOptions{
            .version = ResourceTemplates::Versions::KBV_ERP_1_1_0,
            .pzn = pzn,
        },
    });
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});

    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, kvnr.id(),
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(std::rethrow_exception(exception), HttpStatus::BadRequest,
                                      "Ungültige PZN: Die übergebene Pharmazentralnummer entspricht nicht den "
                                      "vorgeschriebenen Prüfziffer-Validierungsregeln.");
}

TEST_F(ActivateTaskTest, failInvalidPZNFormat)
{
    const model::Kvnr kvnr{"B123457897"};
    const model::Pzn shortPZN{"2750891"};
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
        .prescriptionId = taskId,
        .authoredOn = signingTime,
        .kvnr = kvnr.id(),
        .medicationOptions{
            .version = ResourceTemplates::Versions::KBV_ERP_1_1_0,
            .pzn = shortPZN,
        },
    });
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});

    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, kvnr.id(),
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(
        std::rethrow_exception(exception),
        "Bundle.entry[4].resource{Medication}.code.coding[0]: "
        "error: -erp-begrenzungPznCode: Der PZN-Code muss aus genau 8 Zeichen bestehen. "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN:pznCode|1.1.0); ");
}

TEST_F(ActivateTaskTest, ERP17605CrashMissingIngredientArray)
{
    auto kbvBundleXml = ResourceManager::instance().getStringResource(
        "test/issues/ERP-17605/Bundle_invalid_MedicationCompounding_missing_ingredient_array.xml");
    const auto taskId =
        model::PrescriptionId::fromString("160.100.000.000.051.83");
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = taskId});

    auto now = model::Timestamp::now();
    kbvBundleXml = String::replaceAll(kbvBundleXml, "2023-12-20", now.toGermanDate());

    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, "H030170228",
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = model::Timestamp::fromGermanDate("2022-05-20"),
        .insertTask=true, .outExceptionPtr = exception}));
    EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(
        std::rethrow_exception(exception),
        "Bundle.entry[2].resource{Medication}.ingredient[*]: error: missing mandatory element (from profile: "
        "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_Compounding|1.1.0); ");
}

class DigaNoAlternativeIdAllowed : public ActivateTaskTest,
                                   public testing::WithParamInterface<std::pair<std::string, std::string>>
{
};
TEST_P(DigaNoAlternativeIdAllowed, test)
{
    A_26372.test("DigaNoAlternativeIdAllowed");
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6001);
    auto evdgaBundleXml = ResourceTemplates::evdgaBundleXml({.prescriptionId = taskId.toString(),
                                                             .kvnr = "X234567891",
                                                             .coverageSystem = GetParam().first,
                                                             .coverageCode = GetParam().second,
                                                             .authoredOn = signingTime.toGermanDate()});

    const auto extensionCoverage =
        R"(<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Alternative_IK"><valueIdentifier><system value=")" +
        std::string{model::resource::naming_system::argeIknr} +
        R"(" /><value value="121191241" /></valueIdentifier></extension>)";

    // inject extension before payor.identifier.system:
    const auto injectBefore = R"(<system value="http://fhir.de/sid/arge-ik/iknr" />)";
    evdgaBundleXml = String::replaceAll(evdgaBundleXml, injectBefore, extensionCoverage + injectBefore);
    ASSERT_NE(evdgaBundleXml.find("KBV_EX_FOR_Alternative_IK"), std::string::npos) << "extension injection failed!";

    std::string accidentExt =
        R"(<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Accident"><extension url="Unfallkennzeichen"><valueCoding><system value="https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"/><code value="4"/></valueCoding></extension></extension>)";
    const auto injectBefore2 = R"(<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_EVDGA_SER">)";
    evdgaBundleXml = String::replaceAll(evdgaBundleXml, injectBefore2, accidentExt + injectBefore2);

    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, evdgaBundleXml, "X234567891",
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    ASSERT_TRUE(exception);
    A_26372.test("no Coverage.payor.identifier.extension:alternativeID allowed");
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(
        std::rethrow_exception(exception), HttpStatus::BadRequest,
        "Workflow nicht für Verordnungen nutzbar, die zu Lasten von Unfallkassen oder Berufsgenossenschaften gehen");
}
INSTANTIATE_TEST_SUITE_P(
    x, DigaNoAlternativeIdAllowed,
    testing::Values(std::make_pair("https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Payor_Type_KBV", "UK"),
                     std::make_pair("http://fhir.de/CodeSystem/versicherungsart-de-basis", "BG")));

TEST_F(ActivateTaskTest, DigaE16D)
{
    A_25991.test("e16D");
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6001);
    auto evdgaBundleXml = ResourceTemplates::evdgaBundleXml(
        {.prescriptionId = taskId.toString(), .kvnr = "X234567891", .authoredOn = signingTime.toGermanDate()});
    evdgaBundleXml = String::replaceAll(evdgaBundleXml, "e16D", "e16A");
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = taskId});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, evdgaBundleXml, "X234567891",
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    ASSERT_TRUE(exception);
    EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(
        std::rethrow_exception(exception),
        R"(; Bundle.entry[0].resource{Composition}.type.coding[0].code: error: value must match fixed value: "e16D" (but is "e16A") (from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_EVDGA_Composition|1.1.0))");
}

TEST_F(ActivateTaskTest, DigaDeviceRequest)
{
    A_25991.test("DeviceRequest-Ressource");
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6001);
    auto evdgaBundleXml = ResourceTemplates::kbvBundleXml({.prescriptionId = taskId, .kvnr = "X234567891"});
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = taskId});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, evdgaBundleXml, "X234567891",
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    ASSERT_TRUE(exception);
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(
        std::rethrow_exception(exception), HttpStatus::BadRequest,
        R"(Für diesen Workflowtypen sind nur Verordnungen für Digitale Gesundheitsanwendungen zulässig)");
}

TEST_F(ActivateTaskTest, DigaPZN)
{
    A_25992.test("PZN-checksum in KBV_PR_EVDGA_HealthAppRequest.codeCodeableConcept.coding.code");
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6001);
    auto evdgaBundleXml = ResourceTemplates::evdgaBundleXml(
        {.prescriptionId = taskId.toString(), .kvnr = "X234567891", .authoredOn = signingTime.toGermanDate()});
    evdgaBundleXml = String::replaceAll(evdgaBundleXml, "19205615", "19205616");
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = taskId});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, evdgaBundleXml, "X234567891",
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    ASSERT_TRUE(exception);
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(
        std::rethrow_exception(exception), HttpStatus::BadRequest,
        R"(Ungültige PZN: Die übergebene Pharmazentralnummer entspricht nicht den vorgeschriebenen Prüfziffer-Validierungsregeln.)");
}


struct ActivateTaskTestDarreichungsformValidityParam
{
    std::string darreichungsfrom;
    std::string becomesValidInView;
    std::string previousVersion;
};

class ActivateTaskTestDarreichungsformValidity
    : public ActivateTaskTest,
      public ::testing::WithParamInterface<ActivateTaskTestDarreichungsformValidityParam>
{
};

TEST_P(ActivateTaskTestDarreichungsformValidity, DarreichungsformNotYetValid)
{
    using namespace fhirtools::version_literal;
    const auto& fhirInstance = Fhir::instance();
    const auto* backend = std::addressof(fhirInstance.backend());
    const auto now = model::Timestamp::now();
    const auto today = date::floor<date::days>(now.toChronoTimePoint());
    const auto tomorrow = today + date::days{1};
    testutils::ShiftFhirResourceViewsGuard shiftView{GetParam().becomesValidInView, tomorrow};
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
    auto kbvBundleXml = ResourceTemplates::kbvBundleXml(ResourceTemplates::KbvBundleOptions{
        .prescriptionId = taskId,
        .medicationOptions =
            {
                .version = ResourceTemplates::Versions::KBV_ERP_current(),
                .darreichungsform = GetParam().darreichungsfrom,
            },
    });

    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, "X234567891",
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = now, .outExceptionPtr = exception}));
    const gsl::not_null repoView = Fhir::instance().structureRepository(now).match(
        backend, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle",
        ResourceTemplates::Versions::KBV_ERP_current());
    const auto* valueSet = repoView->findValueSet(
        fhirtools::DefinitionKey{"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM", std::nullopt});
    ASSERT_NE(valueSet, nullptr);
    ASSERT_EQ(valueSet->getVersion(), fhirtools::FhirVersion{GetParam().previousVersion}) << repoView->id();
    ASSERT_FALSE(valueSet->containsCode(GetParam().darreichungsfrom,
                                        "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM")) << repoView->id();
    // Bonbons and bubble gum should be there, right?
    ASSERT_TRUE(valueSet->containsCode("BON", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
    ASSERT_TRUE(valueSet->containsCode("KGU", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
    EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(
        std::rethrow_exception(exception),
        "Bundle.entry[4].resource{Medication}.form.coding[0]: error: Code " + GetParam().darreichungsfrom +
            " is not part of CodeSystem "
            "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM (from profile: "
            "http://hl7.org/fhir/StructureDefinition/Coding|4.0.1); "
            "Bundle.entry[4].resource{Medication}.form.coding[0]: "
            "error: Code " +
            GetParam().darreichungsfrom +
            " is not part of CodeSystem "
            "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM "
            "(from profile: http://hl7.org/fhir/StructureDefinition/CodeableConcept|4.0.1); "
            "Bundle.entry[4].resource{Medication}.form.coding[0]: error: Code " +
            GetParam().darreichungsfrom +
            " is not part of CodeSystem "
            "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM (from profile: "
            "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN|1.1.0); "
            "Bundle.entry[4].resource{Medication}.form.coding[0]: "
            "error: Code " +
            GetParam().darreichungsfrom +
            " with system https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM not allowed "
            "for "
            "ValueSet https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM|" + GetParam().previousVersion + ", allowed are " +
            valueSet->codesToString() +
            " (from profile: "
            "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN:kbvDarreichungsform|1.1.0); ");
}

TEST_P(ActivateTaskTestDarreichungsformValidity, DarreichungsformValid)
{
    auto today = date::floor<date::days>(std::chrono::system_clock::now());
    testutils::ShiftFhirResourceViewsGuard shiftView{GetParam().becomesValidInView, today};

    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
    auto kbvBundleXml = ResourceTemplates::kbvBundleXml(ResourceTemplates::KbvBundleOptions{
        .prescriptionId = taskId,
        .medicationOptions =
            {
                .version = ResourceTemplates::Versions::KBV_ERP_current(),
                .darreichungsform = GetParam().darreichungsfrom,
            },
    });

    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, taskJson, kbvBundleXml, "X234567891",
                          {.expectedStatus = HttpStatus::OK, .signingTime = model::Timestamp::now()}));
}

INSTANTIATE_TEST_SUITE_P(darreichnungsformen, ActivateTaskTestDarreichungsformValidity,
                         ::testing::ValuesIn(std::list<ActivateTaskTestDarreichungsformValidityParam>{
                             {.darreichungsfrom = "LYE", .becomesValidInView = "KBV_2023-07-01_2025-Q2", .previousVersion = "1.14"},
                             {.darreichungsfrom = "PUE", .becomesValidInView = "KBV_2023-07-01_2025-Q2", .previousVersion = "1.14"},
                         }),
                         [](const auto& p) {
                             return p.param.darreichungsfrom;
                         });

namespace
{
struct Parameters {
    std::string_view testName;
    std::string_view signingTime;
    std::string_view acceptDate;
    std::string_view expiryDate;
    std::string_view acceptDatePkv;
};


using ParameterTuple = std::tuple<Parameters, model::PrescriptionId>;

std::ostream& operator<<(std::ostream& out, const Parameters& params)
{
    out << params.testName;
    return out;
}
}// namespace

class ProzessParameterFlowtype : public ActivateTaskTest, public testing::WithParamInterface<ParameterTuple>
{
};


TEST_P(ProzessParameterFlowtype, samples)
{
    std::vector<EnvironmentVariableGuard> envVars;
    const auto* kvnr = "X234567891";
    testutils::ShiftFhirResourceViewsGuard shiftView(
        "EVDGA_2025_01_15", date::year_month_day{date::year(2020), date::month(02), date::day(01)});

    A_19445_10.test("Prozessparameter - Flowtype");
    //using namespace model::resource;
    const auto& [params, prescriptionId] = GetParam();
    auto signingTime = model::Timestamp::fromGermanDate(std::string{params.signingTime});
    auto acceptDate = model::Timestamp::fromGermanDate(std::string{params.acceptDate});
    auto expiryDate = model::Timestamp::fromGermanDate(std::string{params.expiryDate});

    if (prescriptionId.isPkv() || isDiga(prescriptionId.type()))
    {
        acceptDate = model::Timestamp::fromGermanDate(std::string{params.acceptDatePkv});
    }

    // Create Task
    const auto taskJson = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId, .timestamp = signingTime});
    std::string kbvBundleXml =
        ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId, .authoredOn = signingTime, .kvnr = kvnr});
    std::string_view flowTypeDisplayName;
    switch (prescriptionId.type())
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
            flowTypeDisplayName = "Muster 16 (Apothekenpflichtige Arzneimittel)";
            break;
        case model::PrescriptionType::digitaleGesundheitsanwendungen:
            flowTypeDisplayName = "Muster 16 (Digitale Gesundheitsanwendungen)";
            kbvBundleXml = ResourceTemplates::evdgaBundleXml({.prescriptionId = prescriptionId.toString(),
                                                              .timestamp = signingTime.toXsDateTime(),
                                                              .kvnr = kvnr,
                                                              .authoredOn = signingTime.toGermanDate()});
            break;
        case model::PrescriptionType::direkteZuweisung:
            flowTypeDisplayName = "Muster 16 (Direkte Zuweisung)";
            break;
        case model::PrescriptionType::direkteZuweisungPkv:
            flowTypeDisplayName = "PKV (Direkte Zuweisung)";
            break;
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            flowTypeDisplayName = "PKV (Apothekenpflichtige Arzneimittel)";
            break;
    }
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, taskJson, kbvBundleXml, kvnr,
                                              {.signingTime = signingTime,
                                               .expectedExpiry = expiryDate,
                                               .expectedAcceptDate = acceptDate,
                                               .flowTypeDisplayName = flowTypeDisplayName}));
}


std::list<Parameters> samples = {
    {"normal"  , "2021-02-10", "2021-03-10", "2021-05-10", "2021-05-10"},
    {"leapYear", "2020-02-10", "2020-03-09", "2020-05-10", "2020-05-10"},
    {"leapDay" , "2020-02-29", "2020-03-28", "2020-05-29", "2020-05-29"}
};

INSTANTIATE_TEST_SUITE_P(
    samples, ProzessParameterFlowtype,
    testing::Combine(
        testing::ValuesIn(samples),
        testing::Values(
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713),
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6001),
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisung, 4718),
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50010),
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 50011))));

struct ActivateTaskTestPatchParam {
    std::string testling;
    std::string expectedError;
};
std::ostream& operator<<(std::ostream& os, const ActivateTaskTestPatchParam& p)
{
    os << p.testling;
    return os;
}
class ActivateTaskTestPatchTest : public ActivateTaskTest,
                                  public ::testing::WithParamInterface<ActivateTaskTestPatchParam>
{
};

TEST_P(ActivateTaskTestPatchTest, ValidateAfterPatch)
{
    auto kbvBundleXml = ResourceManager::instance().getStringResource(
        "test/validation/xml/v_2023_07_01/kbv/bundle_1_1_2/" + GetParam().testling);

    const auto tomorrow = (model::Timestamp::now() + date::days{1}).toXsDate(model::Timestamp::GermanTimezone);
    kbvBundleXml = String::replaceAll(kbvBundleXml, "<authoredOn value=\"2022-05-20\" />",
                                      "<authoredOn value=\"" + tomorrow + "\" />");
    auto signingTime = model::Timestamp::fromGermanDate(tomorrow);
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft,
                                     .prescriptionId = model::PrescriptionId::fromString("160.100.000.000.004.30"),
                                     .timestamp = signingTime});
    std::exception_ptr exceptionPtr;
    checkActivateTask(mServiceContext, taskJson, kbvBundleXml, "K220635158",
                      {.expectedStatus = HttpStatus::BadRequest, .insertTask = true, .outExceptionPtr = exceptionPtr});

    EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(std::rethrow_exception(exceptionPtr), GetParam().expectedError);
}

INSTANTIATE_TEST_SUITE_P(
    x, ActivateTaskTestPatchTest,
    ::testing::Values(
        ActivateTaskTestPatchParam{
            .testling = "Bundle_invalid_Beispiel_6_URN_-erp-angabeIdentifikatorVerantwortlichePerson_Negativ.xml",
            .expectedError =
                "Bundle: error: -erp-angabeIdentifikatorVerantwortlichePerson: In der Ressource vom Typ Practitioner "
                "ist "
                "der Identifikator der verantwortlichen Person nicht vorhanden, dieser ist aber eine Pflichtangabe bei "
                "den Kostentraegern der Typen \"GKV\", \"BG\", \"SKT\" oder \"UK\", wenn es sich um einen Arzt, "
                "Zahnarzt "
                "oder Arzt als Vertreter handelt und keine ASV-Fachgruppennummer angegeben ist. (from profile: "
                "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0); "},
        ActivateTaskTestPatchParam{
            .testling =
                "Bundle_invalid_Beispiel_60_URN_-erp-angabeFachgruppennummerAsvAusstellendePersonVerbot_Negativ.xml",
            .expectedError =
                "Bundle: error: -erp-angabeFachgruppennummerAsvAusstellendePersonVerbot: In einer Ressource vom Typ "
                "Practitioner ist eine ASV-Fachgruppennummer der ausstellenden Person vorhanden, diese darf aber nur "
                "angegeben werden, wenn die Rechtsgrundlage den Wert \"01\" oder \"11\" besitzt und wenn es sich um "
                "einen Arzt oder Arzt als Vertreter handelt, f\xC3\xBCr den kein Identifikator angegeben ist. (from "
                "profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0); "},
        ActivateTaskTestPatchParam{
            .testling = "Bundle_invalid_Beispiel_6_URN_-erp-angabeVerantwortlichePersonVerbot-2_Negativ.xml",
            .expectedError = "Bundle: error: -erp-angabeVerantwortlichePersonVerbot-2: Eine Ressource vom Typ "
                             "Practitioner wird als verantwortliche Person angegeben, diese darf aber nur angegeben "
                             "werden, wenn es sich nicht um eine Hebamme oder einen Arzt in Weiterbildung handelt. "
                             "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0); "},
        ActivateTaskTestPatchParam{
            .testling =
                "Bundle_invalid_Beispiel_60_URN_-erp-angabeFachgruppennummerAsvVerantwortlichePersonVerbot_Negativ.xml",
            .expectedError =
                "Bundle: error: -erp-angabeFachgruppennummerAsvVerantwortlichePersonVerbot: In einer Ressource vom Typ "
                "Practitioner ist eine ASV-Fachgruppennummer der verantwortlichen Person vorhanden, diese darf aber "
                "nur angegeben werden, wenn die Rechtsgrundlage den Wert \"01\" oder \"11\" besitzt und wenn es sich "
                "um einen Arzt oder Arzt als Vertreter handelt, f\xC3\xBCr den kein Identifikator angegeben ist. (from "
                "profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0); "},
        ActivateTaskTestPatchParam{
            .testling = "Bundle_invalid_Beispiel_6_URN_-erp-angabeVerantwortlichePersonVerbot-1_Negativ_1.xml",
            .expectedError =
                "Bundle: error: -erp-angabeVerantwortlichePersonVerbot-1: Eine Ressource vom Typ Practitioner wird als "
                "verantwortliche Person angegeben, diese darf aber nur angegeben werden, wenn es sich bei der "
                "ausstellenden Person um einen Arzt in Weiterbildung oder Arzt als Vertreter handelt. (from profile: "
                "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0); "},
        ActivateTaskTestPatchParam{
            .testling = "Bundle_invalid_Beispiel_1_URN_-erp-angabeIdentifikatorAusstellendePerson_Negativ.xml",
            .expectedError =
                "Bundle: error: -erp-angabeIdentifikatorAusstellendePerson: In der Ressource vom Typ Practitioner ist "
                "der Identifikator der ausstellenden oder verschreibenden Person nicht vorhanden, dieser ist aber eine "
                "Pflichtangabe bei den Kostentraegern der Typen \"GKV\", \"BG\", \"SKT\", \"UK\" oder \"PKV\", wenn es "
                "sich um einen Arzt, Zahnarzt oder Arzt als Vertreter handelt und keine ASV-Fachgruppennummer "
                "angegeben ist. (from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0); "}));
