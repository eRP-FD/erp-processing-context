/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "DosageInstructionTestHelper.hxx"
#include "erp/model/Task.hxx"
#include "erp/service/MedicationDispenseHandler.hxx"
#include "erp/service/task/CloseTaskHandler.hxx"
#include "erp/service/task/DispenseTaskHandler.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/model/MedicationDispense.hxx"
#include "shared/model/MedicationDispenseBundle.hxx"
#include "shared/model/MedicationDispenseOperationParameters.hxx"
#include "shared/util/Demangle.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"
#include "test/erp/service/EndpointHandlerTest/MedicationDispenseFixture.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>


/**
 * @brief Endpoint tests of DispenseTaskHandler
 *
 */
class DispenseTaskTest : public MedicationDispenseFixture
{
protected:

    static const inline model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715);

    std::string secret = "";
    model::Task::Status taskStatus = model::Task::Status::draft;

    void SetUp() override
    {
        using namespace ResourceTemplates;

        secret = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
        taskStatus = model::Task::Status::inprogress;
    }

    void resetTask(SessionContext& sessionContext)
    {
        auto database = sessionContext.serviceContext.databaseFactory();
        auto taskAndKey = database->retrieveTaskForUpdate(prescriptionId);
        ASSERT_TRUE(taskAndKey.has_value());
        auto& task = taskAndKey->task;
        task.setStatus(taskStatus);
        database->updateTaskStatusAndSecret(task);
        database->commitTransaction();
    }

    ServerRequest createRequest(std::string body)
    {
        Header requestHeader{HttpMethod::POST,
                             "/Task/" + prescriptionId.toString() + "/$dispense/",
                             0,
                             {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                             HttpStatus::Unknown};
        ServerRequest serverRequest{std::move(requestHeader)};
        serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
        serverRequest.setAccessToken(std::move(jwtPharmacy));
        serverRequest.setQueryParameters({{"secret", secret}});
        serverRequest.setBody(std::move(body));
        return serverRequest;
    }

    void test(std::string body, ExpectedResult expectedResult = ExpectedResult::success,
              const std::function<void(SessionContext&)>& additionalTest = [](SessionContext&){})
    {
        DispenseTaskHandler handler({});
        ServerRequest serverRequest = createRequest(std::move(body));
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_NO_FATAL_FAILURE(resetTask(sessionContext));
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        switch (expectedResult)
        {
            case ExpectedResult::success:
                ASSERT_NO_ERP_EXCEPTION(handler.handleRequest(sessionContext));
                EXPECT_EQ(sessionContext.response.getHeader().status(), HttpStatus::NoContent);
                EXPECT_TRUE(sessionContext.response.getBody().empty());
                break;
            case ExpectedResult::failure:
                // clang-format off
                ASSERT_NO_THROW(
                    try
                    {
                        handler.handleRequest(sessionContext);
                        ADD_FAILURE() << "Expected ErpException.";
                    }
                    catch (const ErpException& ex)
                    {
                        EXPECT_EQ(ex.status(), HttpStatus::BadRequest) << ex.what();
                    }
                );
                // clang-format on
                break;
            case ExpectedResult::noCatch:
                handler.handleRequest(sessionContext);
        }
        try {
            auto* database = sessionContext.database();
            auto [taskAndKey, prescription] = database->retrieveTaskForUpdateAndPrescription(prescriptionId);
            ASSERT_TRUE(taskAndKey.has_value());
            additionalTest(sessionContext);
        }
        catch (...)
        {
            FAIL() << "caught unknown exception while handling additional tests";
        }
    }

    void testAndVerifyException(const std::string body, const HttpStatus httpStatus, const std::string expectedMessage)
    {
        try
        {
            ASSERT_NO_FATAL_FAILURE(test(body, ExpectedResult::noCatch));
            FAIL() << "expected exception";
        }
        catch (const ErpException& ex)
        {
            EXPECT_EQ(ex.status(), httpStatus);
            EXPECT_STREQ(ex.what(), expectedMessage.c_str());
        }
        catch (const std::exception& ex)
        {
            FAIL() << "Unexpected exception: " << util::demangle(typeid(ex).name()) << ": " << ex.what();
        }
    }

    std::string dispenseTaskBody(std::list<ResourceTemplates::MedicationDispenseOptions> medicationDispenseOptions)
    {
        auto gemVersion = ResourceTemplates::Versions::GEM_ERP_current();
        return ResourceTemplates::medicationDispenseOperationParametersXml({
            .profileType = model::ProfileType::GEM_ERP_PR_PAR_DispenseOperation_Input,
            .version = gemVersion,
            .medicationDispenses = std::move(medicationDispenseOptions),
        });
    }
};

TEST_F(DispenseTaskTest, DispenseTask)//NOLINT(readability-function-cognitive-complexity)
{
    A_24283_03.test("Dispensierinformationen bereitstellen - Speicherung mehrerer MedicationDispenses");
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = telematikId};
    const auto body = dispenseTaskBody({dispenseOptions});
    ASSERT_NO_FATAL_FAILURE(test(body));
}

TEST_F(DispenseTaskTest, wrongSecret)//NOLINT(readability-function-cognitive-complexity)
{
    A_24280.test("E-Rezept-Fachdienst - Prüfung Secret");
    secret = "wrong_secret";
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = telematikId};
    const auto body = dispenseTaskBody({dispenseOptions});
    testAndVerifyException(body, HttpStatus::Forbidden, "No or invalid secret provided for Task");
}

TEST_F(DispenseTaskTest, wrongTaskStatus)//NOLINT(readability-function-cognitive-complexity)
{
    A_24298.test("E-Rezept-Fachdienst - Prüfung Status");
    taskStatus = model::Task::Status::ready;
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = telematikId};
    const auto body = dispenseTaskBody({dispenseOptions});
    testAndVerifyException(body, HttpStatus::Forbidden, "Task not in-progress.");
}

TEST_F(DispenseTaskTest, PrescriptionId)//NOLINT(readability-function-cognitive-complexity)
{
    A_24281_02.test("E-Rezept-Fachdienst - Korrektheit Rezept-ID");
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = "160.000.000.000.777.xx",
                                                                       .telematikId = telematikId};
    const auto body = dispenseTaskBody({dispenseOptions});
    ASSERT_NO_FATAL_FAILURE(test(body, ExpectedResult::failure));
}



TEST_F(DispenseTaskTest, KVNR)//NOLINT(readability-function-cognitive-complexity)
{
    A_24281_02.test("E-Rezept-Fachdienst - Korrektheit KVNR");
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .kvnr = "X234567777",
                                                                       .telematikId = telematikId};
    const auto body = dispenseTaskBody({dispenseOptions});
    ASSERT_NO_FATAL_FAILURE(test(body, ExpectedResult::failure));
}

TEST_F(DispenseTaskTest, TelematikId)//NOLINT(readability-function-cognitive-complexity)
{
    A_24281_02.test("E-Rezept-Fachdienst - Korrektheit performer.actor");
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = "3-SMC-B-Testkarte-883110000120000"};
    const auto body = dispenseTaskBody({dispenseOptions});
    ASSERT_NO_FATAL_FAILURE(test(body, ExpectedResult::failure));
}

TEST_F(DispenseTaskTest, MultiDispenseTask)//NOLINT(readability-function-cognitive-complexity)
{
    // A_24283 Speicherung mehrerer MedicationDispenses
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = telematikId};
    const auto body = dispenseTaskBody({dispenseOptions, dispenseOptions});
    ASSERT_NO_FATAL_FAILURE(test(body));
}

TEST_F(DispenseTaskTest, Zeitstempel)//NOLINT(readability-function-cognitive-complexity)
{
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = telematikId};
    const auto body = dispenseTaskBody({dispenseOptions});

    auto additionalTest = [](SessionContext& sessionContext) {
        auto* database = sessionContext.database();
        auto [taskAndKey, prescription] = database->retrieveTaskForUpdateAndPrescription(prescriptionId);
        ASSERT_TRUE(taskAndKey.has_value());
        model::Task& savedTask = taskAndKey->task;
        // A_24285 Zeitstempel angelegt
        ASSERT_TRUE(savedTask.lastMedicationDispense().has_value());
    };
    ASSERT_NO_FATAL_FAILURE(test(body, ExpectedResult::success, additionalTest));
}

TEST_F(DispenseTaskTest, SameStatus)//NOLINT(readability-function-cognitive-complexity)
{
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = telematikId};
    const auto body = dispenseTaskBody({dispenseOptions});

    auto additionalTest = [](SessionContext& sessionContext) {
        auto* database = sessionContext.database();
        auto [taskAndKey, prescription] = database->retrieveTaskForUpdateAndPrescription(prescriptionId);
        ASSERT_TRUE(taskAndKey.has_value());
        model::Task& savedTask = taskAndKey->task;
        // A_24284 Keine Statusänderung
        EXPECT_EQ(savedTask.status(), model::Task::Status::inprogress);
    };
    ASSERT_NO_FATAL_FAILURE(test(body, ExpectedResult::success, additionalTest));
}

TEST_F(DispenseTaskTest, DispenseSalt)//NOLINT(readability-function-cognitive-complexity)
{
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = telematikId};
    const auto body = dispenseTaskBody({dispenseOptions});

    auto additionalTest = [](SessionContext& sessionContext) {
        try
        {
            auto mockBackend = dynamic_cast<MockDatabaseProxy&>(sessionContext.database()->getBackend());
            auto medicationDispenseSalt = mockBackend.retrieveMedicationDispenseSalt(prescriptionId);
            EXPECT_TRUE(medicationDispenseSalt.has_value());
        }
        catch(const std::exception& e)
        {
            FAIL() << "unexpected exception: " << e.what();
        }
    };
    ASSERT_NO_FATAL_FAILURE(test(body, ExpectedResult::success, additionalTest));
}

TEST_F(DispenseTaskTest, OwnerUpdated)
{
    std::string otherTelematikId("3-SMC-B-Testkarte-883110000120999");
    ASSERT_NE(telematikId, otherTelematikId);
    jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke(otherTelematikId);
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = otherTelematikId};
    const auto body = dispenseTaskBody({dispenseOptions});

    auto additionalTest = [&](SessionContext& sessionContext) {
        auto task = sessionContext.database()->retrieveTaskForUpdate(prescriptionId);
        ASSERT_TRUE(task.has_value());
        EXPECT_EQ(task->task.owner().value_or(""), otherTelematikId);
    };
    ASSERT_NO_FATAL_FAILURE(test(body, ExpectedResult::success, additionalTest));
}

class DispenseTaskEmptyClose_ERP_23917;

struct  DispenseTaskEmptyCloseParam
{
    enum ReturnType {
        MedicationDispense,
        MedicationDispenseBundle,
    };
    ReturnType getReturns;
    size_t expectedDispenseCount;
    size_t expectedMedicationCount;
};

class DispenseTaskEmptyClose_ERP_23917 : public DispenseTaskTest, public testing::WithParamInterface<DispenseTaskEmptyCloseParam>
{
public:
    void closeTask()
    {
        Header requestHeader{HttpMethod::POST,
                             "/Task/" + prescriptionId.toString() + "/$close/",
                             0,
                             {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                             HttpStatus::Unknown};
        ServerRequest serverRequest{std::move(requestHeader)};
        serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
        serverRequest.setAccessToken(std::move(jwtPharmacy));
        serverRequest.setQueryParameters(
            {{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});
        CloseTaskHandler handler({});
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    }
};

TEST_P(DispenseTaskEmptyClose_ERP_23917, closeEmptyBody)
{
    using namespace std::string_literals;
    auto input = medicationDispenseBody({EndpointType::dispense});
    ASSERT_NO_FATAL_FAILURE(test(input, ExpectedResult::success)) << input;
    ASSERT_NO_FATAL_FAILURE(closeTask());
    std::optional<model::UnspecifiedResource> unspec;
    ASSERT_NO_FATAL_FAILURE(unspec = getMedicationDispenses("X234567891", prescriptionId));
    ASSERT_TRUE(unspec.has_value());
    switch (GetParam().getReturns)
    {
        case DispenseTaskEmptyCloseParam::MedicationDispense:
            break;
        case DispenseTaskEmptyCloseParam::MedicationDispenseBundle: {
            auto mdBundle = model::MedicationDispenseBundle::fromJson(std::move(unspec).value().jsonDocument());
            EXPECT_EQ(mdBundle.getResourcesByType<model::MedicationDispense>().size(),
                      GetParam().expectedDispenseCount) << mdBundle.serializeToJsonString();
            EXPECT_EQ(mdBundle.getResourcesByType<model::UnspecifiedResource>("Medication").size(),
                      GetParam().expectedMedicationCount) ;
            unspec = model::UnspecifiedResource::fromJson(std::move(mdBundle).jsonDocument());
            break;
        }
        default:
            FAIL();
    }
    if (HasFailure())
    {
        LOG(INFO) << unspec->serializeToJsonString();
    }
}

INSTANTIATE_TEST_SUITE_P(input, DispenseTaskEmptyClose_ERP_23917,
                         testing::ValuesIn(std::initializer_list<DispenseTaskEmptyCloseParam>{
                             {
                                 .getReturns = DispenseTaskEmptyCloseParam::MedicationDispenseBundle,
                                 .expectedDispenseCount = 2,
                                 .expectedMedicationCount = 2,
                             }
                         }));




class DispenseTaskProfileValidityTest : public DispenseTaskTest,
                                        public ::testing::WithParamInterface< MedicationDispenseFixture::ProfileValidityTestParams>
{
};

TEST_P(DispenseTaskProfileValidityTest, run)
{
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseBody({
                                     EndpointType::dispense,
                                     GetParam().version,
                                     {testutils::shiftDate(GetParam().date)},
                                 }),
                                 GetParam().result));
}

INSTANTIATE_TEST_SUITE_P(parameters, DispenseTaskProfileValidityTest,
                         ::testing::ValuesIn(MedicationDispenseFixture::profileValidityTestParameters()));

class DispenseTaskMaxWhenHandedOverTest : public DispenseTaskTest,
public ::testing::WithParamInterface< MedicationDispenseFixture::MaxWhenHandedOverTestParams>
{
};

TEST_P(DispenseTaskMaxWhenHandedOverTest, run)
{
    std::list<std::string> overrideWhenHandedOver;
    std::ranges::transform(GetParam().whenHandedOver, back_inserter(overrideWhenHandedOver), &testutils::shiftDate);
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseBody({
                                     EndpointType::dispense,
                                     GetParam().version,
                                     overrideWhenHandedOver,
                                 }),
                                 GetParam().result));
}

INSTANTIATE_TEST_SUITE_P(parameters, DispenseTaskMaxWhenHandedOverTest,
                         ::testing::ValuesIn(MedicationDispenseFixture::maxWhenHandedOverTestParameters()));


class DispenseTaskDosageValidatorTest : public DispenseTaskTest
{
public:
    DispenseTaskDosageValidatorTest()
        : handler({})
        , requestHeader({HttpMethod::POST,
                         "/Task/" + prescriptionId.toString() + "/$dispense/",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                         HttpStatus::Unknown})
        , serverRequest(requestHeader)
        , sessionContext(mServiceContext, serverRequest, serverResponse, accessLog)

    {
        serverRequest.setAccessToken(jwtPharmacy);
        serverRequest.setQueryParameters(
            {{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});
        mockDatabase.reset();
        mServiceContext.databaseFactory();// re-init mockdb
        auto task = mockDatabase->retrieveTaskForUpdate(prescriptionId);
        serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
    }
    model::MedicationDispenseOperationParameters getSample()
    {
        return DosageInstructionTestHelper::getDispenseSample(
            prescriptionId, "valid/MedicationDispense-MD-Dosage-1020.json",
            model::ProfileType::GEM_ERP_PR_PAR_DispenseOperation_Input);
    }
    DispenseTaskHandler handler;
    Header requestHeader;
    ServerRequest serverRequest;
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext;
    testutils::ShiftFhirResourceViewsGuard shift{"KBV_1_4", date::floor<date::days>(std::chrono::system_clock::now())};
};

TEST_F(DispenseTaskDosageValidatorTest, wrongScriptVersion)
{
    auto sample = getSample();
    const rapidjson::Pointer ptr("/parameter/0/part/0/resource/extension/1/extension/0/valueString");
    sample.setValue(ptr, "1");
    serverRequest.setBody(sample.serializeToXmlString());
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(handler.handleRequest(sessionContext), HttpStatus::BadRequest,
                                  "Validation of rendered dosage-instructions: The algorithm version 1 is unknown");
}

TEST_F(DispenseTaskDosageValidatorTest, wrongLanguage)
{
    auto sample = getSample();
    const rapidjson::Pointer ptr("/parameter/0/part/0/resource/extension/1/extension/1/valueCode");
    sample.setValue(ptr, "de");
    serverRequest.setBody(sample.serializeToXmlString());
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(handler.handleRequest(sessionContext), HttpStatus::BadRequest,
                                      "Validation of rendered dosage-instructions: The language de is unknown");
}

TEST_F(DispenseTaskDosageValidatorTest, wrongText)
{
    auto sample = getSample();
    const rapidjson::Pointer ptr("/parameter/0/part/0/resource/extension/0/valueMarkdown");
    sample.setValue(ptr, "1-0-2-0  Stück");
    serverRequest.setBody(sample.serializeToXmlString());
    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(
        handler.handleRequest(sessionContext), HttpStatus::BadRequest,
        "Validation of rendered dosage-instructions: valueMarkdown does not match the expected string in extension "
        "http://hl7.org/fhir/5.0/StructureDefinition/extension-MedicationDispense.renderedDosageInstruction",
        "expected: 1-0-2-0 Stück");
}
