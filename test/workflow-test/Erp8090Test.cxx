/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/OperationOutcome.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include "test/util/ResourceManager.hxx"


class Erp8090Test : public ErpWorkflowTest
{
};

TEST_F(Erp8090Test, validCoverage1)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    auto oldPrescriptionId = std::string{"160.000.000.004.713.80"};
    auto newPrescriptionId = task->prescriptionId().toString();

    auto& resourceManager = ResourceManager::instance();

    auto bundle = resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_erp8090.xml");
    bundle = String::replaceAll(bundle, "###REPLACE_SYSTEM###", "http://fhir.de/CodeSystem/versicherungsart-de-basis");
    bundle = String::replaceAll(bundle, "###REPLACE_CODE###", "GKV");
    bundle = String::replaceAll(bundle, oldPrescriptionId, newPrescriptionId);
    ASSERT_NO_FATAL_FAILURE(taskActivate(task->prescriptionId(), std::string{task->accessCode()},
                                         toCadesBesSignature(bundle, model::Timestamp::fromXsDate("2021-06-08")),
                                         HttpStatus::OK));

    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    bundle = resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_erp8090.xml");
    bundle = String::replaceAll(bundle, "###REPLACE_SYSTEM###", "http://fhir.de/CodeSystem/versicherungsart-de-basis");
    bundle = String::replaceAll(bundle, "###REPLACE_CODE###", "SEL");
    newPrescriptionId = task->prescriptionId().toString();
    bundle = String::replaceAll(bundle, oldPrescriptionId, newPrescriptionId);
    ASSERT_NO_FATAL_FAILURE(taskActivate(task->prescriptionId(), std::string{task->accessCode()},
                                         toCadesBesSignature(bundle, model::Timestamp::fromXsDate("2021-06-08")),
                                         HttpStatus::OK));

    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    bundle = resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_erp8090.xml");
    bundle = String::replaceAll(bundle, "###REPLACE_SYSTEM###", "http://fhir.de/CodeSystem/versicherungsart-de-basis");
    bundle = String::replaceAll(bundle, "###REPLACE_CODE###", "PKV");
    newPrescriptionId = task->prescriptionId().toString();
    bundle = String::replaceAll(bundle, oldPrescriptionId, newPrescriptionId);
    ASSERT_NO_FATAL_FAILURE(taskActivate(task->prescriptionId(), std::string{task->accessCode()},
                                         toCadesBesSignature(bundle, model::Timestamp::fromXsDate("2021-06-08")),
                                         HttpStatus::OK));
}

TEST_F(Erp8090Test, validCoverage2)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    auto oldPrescriptionId = std::string{"160.000.000.004.713.80"};
    auto newPrescriptionId = task->prescriptionId().toString();

    auto& resourceManager = ResourceManager::instance();

    auto bundle = resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_erp8090_with_KBV_EX_FOR_Alternative_IK.xml");
    bundle = String::replaceAll(bundle, "###REPLACE_SYSTEM###", "http://fhir.de/CodeSystem/versicherungsart-de-basis");
    bundle = String::replaceAll(bundle, "###REPLACE_CODE###", "BG");
    bundle = String::replaceAll(bundle, oldPrescriptionId, newPrescriptionId);
    ASSERT_NO_FATAL_FAILURE(taskActivate(task->prescriptionId(), std::string{task->accessCode()},
                                         toCadesBesSignature(bundle, model::Timestamp::fromXsDate("2021-06-08")),
                                         HttpStatus::OK));

    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    bundle = resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_erp8090_with_KBV_EX_FOR_Alternative_IK.xml");
    bundle = String::replaceAll(bundle, "###REPLACE_SYSTEM###", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Payor_Type_KBV");
    bundle = String::replaceAll(bundle, "###REPLACE_CODE###", "UK");
    newPrescriptionId = task->prescriptionId().toString();
    bundle = String::replaceAll(bundle, oldPrescriptionId, newPrescriptionId);
    ASSERT_NO_FATAL_FAILURE(taskActivate(task->prescriptionId(), std::string{task->accessCode()},
                                         toCadesBesSignature(bundle, model::Timestamp::fromXsDate("2021-06-08")),
                                         HttpStatus::OK));
}

TEST_F(Erp8090Test, invalidCoverage)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    auto oldPrescriptionId = std::string{"160.000.000.004.713.80"};
    auto newPrescriptionId = task->prescriptionId().toString();

    auto& resourceManager = ResourceManager::instance();

    auto bundle = resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_erp8090.xml");
    bundle = String::replaceAll(bundle, "###REPLACE_SYSTEM###", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Payor_Type_KBV");
    bundle = String::replaceAll(bundle, "###REPLACE_CODE###", "SKT");
    bundle = String::replaceAll(bundle, oldPrescriptionId, newPrescriptionId);
    ASSERT_NO_FATAL_FAILURE(taskActivate(task->prescriptionId(), std::string{task->accessCode()},
                                         toCadesBesSignature(bundle, model::Timestamp::fromXsDate("2021-06-08")),
                                         HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                                         "Kostenträger nicht zulässig"));

    EnvironmentVariableGuard disablePkv{ConfigurationKey::FEATURE_PKV, "false"};
    EnvironmentVariableGuard disableWf200{ConfigurationKey::FEATURE_WORKFLOW_200, "false"};

    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    bundle = resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_erp8090.xml");
    bundle = String::replaceAll(bundle, "###REPLACE_SYSTEM###", "http://fhir.de/CodeSystem/versicherungsart-de-basis");
    bundle = String::replaceAll(bundle, "###REPLACE_CODE###", "PKV");
    newPrescriptionId = task->prescriptionId().toString();
    bundle = String::replaceAll(bundle, oldPrescriptionId, newPrescriptionId);
    ASSERT_NO_FATAL_FAILURE(taskActivate(task->prescriptionId(), std::string{task->accessCode()},
                                         toCadesBesSignature(bundle, model::Timestamp::fromXsDate("2021-06-08")),
                                         HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                                         "Kostenträger nicht zulässig"));
}
