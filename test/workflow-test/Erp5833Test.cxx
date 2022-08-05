/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <gtest/gtest.h>

class Erp5833Test : public ErpWorkflowTest
{

};

TEST_F(Erp5833Test, run)//NOLINT(readability-function-cognitive-complexity)
{
    std::string kvnr;
    ASSERT_NO_FATAL_FAILURE(generateNewRandomKVNR(kvnr));
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    std::string accessCode{task->accessCode()};
    const auto [qesBundle, _] = makeQESBundle(kvnr, task->prescriptionId(), fhirtools::Timestamp::now());
    ASSERT_NO_FATAL_FAILURE(task = taskActivate(task->prescriptionId(), accessCode, qesBundle));
    std::optional<model::Bundle> taskBundle;
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskAccept(task->prescriptionId(), accessCode));
    ASSERT_TRUE(taskBundle.has_value());
    ClientResponse innerResponse;
    auto taskRejectWithoutSecret = "/Task/" + task->prescriptionId().toString() + "/$reject?secret=";

    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, innerResponse) =
            send(RequestArguments{HttpMethod::POST, taskRejectWithoutSecret, {}, "application/fhir+xml"}
                .withJwt(jwtApotheke()).withExpectedInnerFlowType("160")
            )
        );
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::Forbidden);
}
