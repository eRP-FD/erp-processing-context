/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <gtest/gtest.h>

class Erp9777Test : public ErpWorkflowTestP
{
};

TEST_P(Erp9777Test, run)//NOLINT(readability-function-cognitive-complexity)
{
    if (!Configuration::instance().featurePkvEnabled())
    {
        GTEST_SKIP();
    }

    std::string kvnr;
    ASSERT_NO_FATAL_FAILURE(generateNewRandomKVNR(kvnr));
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(GetParam()));
    ASSERT_TRUE(task.has_value());
    std::string accessCode{task->accessCode()};
    const auto qesBundle = std::get<0>(makeQESBundle(kvnr, task->prescriptionId(), model::Timestamp::now()));
    ASSERT_NO_FATAL_FAILURE(task = taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode, qesBundle));
    std::optional<model::Bundle> taskBundle;
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskAccept(task->prescriptionId(), accessCode));
    ASSERT_TRUE(taskBundle.has_value());
    std::vector<model::Task> tasks = taskBundle->getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    auto telematikId = jwtApotheke().stringForClaim(JWT::idNumberClaim);
    ASSERT_TRUE(telematikId.has_value());
    auto secret = tasks[0].secret();
    ASSERT_TRUE(secret.has_value());
    using IssueType = model::OperationOutcome::Issue::Type;
    ASSERT_NO_FATAL_FAILURE(chargeItemPost(tasks[0].prescriptionId(), kvnr, *telematikId, std::string{*secret},
                                           HttpStatus::Conflict, IssueType::conflict));
}

INSTANTIATE_TEST_SUITE_P(Erp9777TestInst, Erp9777Test,
                         testing::Values(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
                                         model::PrescriptionType::direkteZuweisungPkv));
