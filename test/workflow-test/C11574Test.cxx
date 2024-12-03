/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <gtest/gtest.h>

class C11574Test : public ErpWorkflowTestP
{
};

TEST_P(C11574Test, successful)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace ResourceTemplates;
    if(Versions::GEM_ERP_current() < Versions::GEM_ERP_1_3)
    {
        GTEST_SKIP();
    }

    std::string kvnr;
    ASSERT_NO_FATAL_FAILURE(generateNewRandomKVNR(kvnr));
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(GetParam()));
    ASSERT_TRUE(task.has_value());
    std::string accessCode{task->accessCode()};
    const auto prescriptionId = task->prescriptionId();
    const auto qesBundle = std::get<0>(makeQESBundle(kvnr, prescriptionId, model::Timestamp::now()));
    ASSERT_NO_FATAL_FAILURE(task = taskActivateWithOutcomeValidation(prescriptionId, accessCode, qesBundle));
    std::optional<model::Bundle> taskBundle;
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskAccept(prescriptionId, accessCode));
    ASSERT_TRUE(taskBundle.has_value());
    std::vector<model::Task> tasks = taskBundle->getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    auto telematikId = jwtApotheke().stringForClaim(JWT::idNumberClaim);
    ASSERT_TRUE(telematikId.has_value());
    auto secret = tasks[0].secret();
    ASSERT_TRUE(secret.has_value());


    const auto dispenseBody =
        dispenseOrCloseTaskBody(model::ProfileType::GEM_ERP_PR_PAR_DispenseOperation_Input, kvnr,
                                prescriptionId.toString(), model::Timestamp::now().toGermanDate(), 1);

    const JWT jwt{ jwtApotheke() };

    // Test that first dispense request succeeds.
    {
        const std::string dispensePath = "/Task/" + prescriptionId.toString() + "/$dispense?secret=" + std::string(secret.value());
        ClientResponse serverResponse;
        ASSERT_NO_FATAL_FAILURE(
            std::tie(std::ignore, serverResponse) =
                send(RequestArguments{HttpMethod::POST, dispensePath, dispenseBody, "application/fhir+xml"}
                    .withJwt(jwt).withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))));
        ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK) << serverResponse.getBody();
    }

    {
        std::optional<model::Bundle> taskBundle;
        ASSERT_NO_FATAL_FAILURE(taskBundle = taskGetId(prescriptionId, kvnr, accessCode));
        ASSERT_TRUE(taskBundle.has_value());
        auto tasks = taskBundle->getResourcesByType<model::Task>("Task");
        ASSERT_EQ(tasks.size(), 1);

        // task must contain the last medication dispense
        ASSERT_TRUE(tasks.at(0).lastMedicationDispense().has_value());
    }

    // Reject Task
    {
        const std::string rejectPath = "/Task/" + prescriptionId.toString() + "/$reject?secret=" + std::string(secret.value());
        ClientResponse serverResponse;
        ASSERT_NO_FATAL_FAILURE(
            std::tie(std::ignore, serverResponse) =
                send(RequestArguments{HttpMethod::POST, rejectPath, {}, "application/fhir+xml"}
                    .withJwt(jwt).withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))));
        ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::NoContent);
    }

    {
        std::optional<model::Bundle> taskBundle;
        ASSERT_NO_FATAL_FAILURE(taskBundle = taskGetId(prescriptionId, kvnr, accessCode));
        ASSERT_TRUE(taskBundle.has_value());
        auto tasks = taskBundle->getResourcesByType<model::Task>("Task");
        ASSERT_EQ(tasks.size(), 1);

        // task must not contain the last medication dispense
        ASSERT_FALSE(tasks.at(0).lastMedicationDispense().has_value());
    }

}

INSTANTIATE_TEST_SUITE_P(C11574TestInst, C11574Test,
                         testing::Values(model::PrescriptionType::apothekenpflichigeArzneimittel,
                                         model::PrescriptionType::direkteZuweisung,
                                         model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
                                         model::PrescriptionType::direkteZuweisungPkv));
