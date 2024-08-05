/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */
#include <gtest/gtest.h>

#include "erp/ErpRequirements.hxx"
#include "erp/model/Task.hxx"
#include "erp/service/task/DispenseTaskHandler.hxx"
#include "erp/util/Demangle.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"
#include "test/util/ResourceTemplates.hxx"

/**
 * @brief Endpoint tests of DispenseTaskHandler
 *
 */
class DispenseTaskTest : public EndpointHandlerTest
{
public:

protected:
    enum class ExpectedResult
    {
        success,
        failure,
        noCatch,
    };

    JWT jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    const std::string telematikId = jwtPharmacy.stringForClaim(JWT::idNumberClaim).value();

    static const inline model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715);

    std::string secret = "";
    model::Task::Status taskStatus = model::Task::Status::draft;

    void SetUp() override
    {
        using namespace ResourceTemplates;
        if(Versions::GEM_ERP_current() < Versions::GEM_ERP_1_3)
        {
            GTEST_SKIP();
        }

        secret = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
        taskStatus = model::Task::Status::inprogress;
    }

    void resetTask(SessionContext& sessionContext)
    {
        auto* database = sessionContext.database();
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
              const std::function<void(model::Task&)>& additionalTest = [](model::Task&){})
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
                ASSERT_NO_THROW(handler.handleRequest(sessionContext));
                break;
            case ExpectedResult::failure:
                ASSERT_ANY_THROW(handler.handleRequest(sessionContext));
                break;
            case ExpectedResult::noCatch:
                handler.handleRequest(sessionContext);
        }
        try {
            auto* database = sessionContext.database();
            auto taskAndKey = database->retrieveTaskForUpdate(prescriptionId);
            ASSERT_TRUE(taskAndKey.has_value());
            model::Task& task = taskAndKey->task;
            additionalTest(task);
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
};

TEST_F(DispenseTaskTest, DispenseTask)//NOLINT(readability-function-cognitive-complexity)
{
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = telematikId};
    const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(dispenseOptions);
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseXml));
}

TEST_F(DispenseTaskTest, wrongSecret)//NOLINT(readability-function-cognitive-complexity)
{
    A_24280.test("E-Rezept-Fachdienst - Prüfung Secret");
    secret = "wrong_secret";
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = telematikId};
    const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(dispenseOptions);
    testAndVerifyException(medicationDispenseXml, HttpStatus::Forbidden, "No or invalid secret provided for Task");
}

TEST_F(DispenseTaskTest, wrongTaskStatus)//NOLINT(readability-function-cognitive-complexity)
{
    A_24298.test("E-Rezept-Fachdienst - Prüfung Status");
    taskStatus = model::Task::Status::ready;
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = telematikId};
    const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(dispenseOptions);
    testAndVerifyException(medicationDispenseXml, HttpStatus::Forbidden, "Task not in-progress.");
}

TEST_F(DispenseTaskTest, PrescriptionId)//NOLINT(readability-function-cognitive-complexity)
{
    A_24281.test("E-Rezept-Fachdienst - Korrektheit Rezept-ID");
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = "160.000.000.000.777.xx",
                                                                       .telematikId = telematikId};
    const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(dispenseOptions);
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseXml, ExpectedResult::failure));
}



TEST_F(DispenseTaskTest, KVNR)//NOLINT(readability-function-cognitive-complexity)
{
    A_24281.test("E-Rezept-Fachdienst - Korrektheit KVNR");
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .kvnr = "X234567777",
                                                                       .telematikId = telematikId};
    const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(dispenseOptions);
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseXml, ExpectedResult::failure));
}

TEST_F(DispenseTaskTest, TelematikId)//NOLINT(readability-function-cognitive-complexity)
{
    A_24281.test("E-Rezept-Fachdienst - Korrektheit performer.actor");
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = "3-SMC-B-Testkarte-883110000120000"};
    const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(dispenseOptions);
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseXml, ExpectedResult::failure));
}

TEST_F(DispenseTaskTest, MultiDispenseTask)//NOLINT(readability-function-cognitive-complexity)
{
    // A_24283 Speicherung mehrerer MedicationDispenses
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = telematikId};
    const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(dispenseOptions);
    const auto medicationDispenseBundleXml =
        ResourceTemplates::medicationDispenseBundleXml({.medicationDispenses = {dispenseOptions, dispenseOptions}});
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseBundleXml));
}

TEST_F(DispenseTaskTest, Zeitstempel)//NOLINT(readability-function-cognitive-complexity)
{
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = telematikId};
    const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(dispenseOptions);

    auto additionalTest = [](model::Task& savedTask) {
        // A_24285 Zeitstempel angelegt
        ASSERT_TRUE(savedTask.lastMedicationDispense().has_value());
    };
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseXml, ExpectedResult::success, additionalTest));
}

TEST_F(DispenseTaskTest, SameStatus)//NOLINT(readability-function-cognitive-complexity)
{
    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = telematikId};
    const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(dispenseOptions);

    auto additionalTest = [](model::Task& savedTask) {
        // A_24284 Keine Statusänderung
        EXPECT_EQ(savedTask.status(), model::Task::Status::inprogress);
    };
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseXml, ExpectedResult::success, additionalTest));
}
