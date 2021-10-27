/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/fhir/Fhir.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/util/Base64.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <gtest/gtest.h>

#include "tools/ResourceManager.hxx"

namespace
{
struct TestParam {
    const std::string_view filename;
    const HttpStatus expectedInnerStatus;
    std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode;
};
}

class Erp6590Test : public ErpWorkflowTestTemplate<::testing::TestWithParam<TestParam>>
{
public:
protected:
    ResourceManager& resourceManager = ResourceManager::instance();
};


TEST_P(Erp6590Test, run)
{
    std::string kbv_bundle_xml = resourceManager.getStringResource(GetParam().filename);
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    std::string accessCode{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivate(task->prescriptionId(), accessCode, toCadesBesSignature(kbv_bundle_xml),
                                         GetParam().expectedInnerStatus, GetParam().expectedErrorCode));
}
// clang-format off
INSTANTIATE_TEST_SUITE_P(samples, Erp6590Test, ::testing::Values(
    TestParam{"test/issues/ERP-6590/kbv_bundle.xml", HttpStatus::OK, std::nullopt},
    TestParam{"test/issues/ERP-6590/kbv_bundle_no_patient_identifier.xml", HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid},
    TestParam{"test/issues/ERP-6590/kbv_bundle_empty_patient_identifier.xml", HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid},
    TestParam{"test/issues/ERP-6590/kbv_bundle_wrongsystem_patient_identifier.xml", HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid},
    TestParam{"test/issues/ERP-6590/kbv_bundle_short_patient_identifier.xml", HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid}));
// clang-format on
