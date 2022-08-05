/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <gtest/gtest.h>

class Erp9777Test : public ErpWorkflowTest
{
};

TEST_F(Erp9777Test, run)//NOLINT(readability-function-cognitive-complexity)
{
    std::string kvnr;
    ASSERT_NO_FATAL_FAILURE(generateNewRandomKVNR(kvnr));
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv));
    ASSERT_TRUE(task.has_value());
    std::string accessCode{task->accessCode()};
    const auto qesBundle = std::get<0>(makeQESBundle(kvnr, task->prescriptionId(), fhirtools::Timestamp::now()));
    ASSERT_NO_FATAL_FAILURE(task = taskActivate(task->prescriptionId(), accessCode, qesBundle));
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
