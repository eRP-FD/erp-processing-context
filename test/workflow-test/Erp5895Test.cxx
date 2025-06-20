/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/Base64.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>



class Erp5895Test : public ErpWorkflowTest
{
protected:
    std::string qesBundle()
    {
        return Base64::encode(resourceManager.getStringResource(testDataPath + "qes_bundle.p7s"));
    }

private:
    ResourceManager& resourceManager = ResourceManager::instance();
    const std::string testDataPath{"test/issues/ERP-5895/"};
};


TEST_F(Erp5895Test, run)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    std::string accessCode{task->accessCode()};
    mActivateTaskRequestArgs.overrideExpectedKbvVersion = "XXX";
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode, qesBundle(),
                                         HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));

}
