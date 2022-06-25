/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_A_22231_BETAEBUNGSMITTELABLEHNEN_H
#define ERP_PROCESSING_CONTEXT_A_22231_BETAEBUNGSMITTELABLEHNEN_H

#include "erp/model/OperationOutcome.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "test/util/ResourceManager.hxx"

class A_22231_BetaebungsmittelAblehnen : public ErpWorkflowTest
{
};

TEST_F(A_22231_BetaebungsmittelAblehnen, category_00_direkteZuweisung)//NOLINT(readability-function-cognitive-complexity)
{
    auto& resourceManager = ResourceManager::instance();
    auto bundle = resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_category_code.xml");
    bundle = String::replaceAll(bundle, "###CODE###", "00");
    bundle = String::replaceAll(bundle, "###COVERAGE###", "GKV");
    bundle = patchVersionsInBundle(bundle);
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::direkteZuweisung));
    ASSERT_TRUE(task.has_value());
    bundle = String::replaceAll(bundle, "###PRESCRIPTIONID###", task->prescriptionId().toString());
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivate(task->prescriptionId(), accessCode,
                                         toCadesBesSignature(bundle, model::Timestamp::fromXsDate("2021-06-08")),
                                         HttpStatus::OK));
}

TEST_F(A_22231_BetaebungsmittelAblehnen, category_01_direkteZuweisung)//NOLINT(readability-function-cognitive-complexity)
{
    auto& resourceManager = ResourceManager::instance();
    auto bundle = resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_category_code.xml");
    bundle = String::replaceAll(bundle, "###CODE###", "01");
    bundle = String::replaceAll(bundle, "###COVERAGE###", "GKV");
    bundle = patchVersionsInBundle(bundle);
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::direkteZuweisung));
    ASSERT_TRUE(task.has_value());
    bundle = String::replaceAll(bundle, "###PRESCRIPTIONID###", task->prescriptionId().toString());
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivate(
        task->prescriptionId(), accessCode, toCadesBesSignature(bundle, model::Timestamp::fromXsDate("2021-06-08")),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid, "BTM und Thalidomid nicht zulässig"));
}

TEST_F(A_22231_BetaebungsmittelAblehnen, category_02_apothekenpflichtigeArzneimittelPkv)//NOLINT(readability-function-cognitive-complexity)
{
    auto& resourceManager = ResourceManager::instance();
    auto bundle = resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_category_code.xml");
    bundle = String::replaceAll(bundle, "###CODE###", "02");
    bundle = String::replaceAll(bundle, "###COVERAGE###", "PKV");
    bundle = patchVersionsInBundle(bundle);
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv));
    ASSERT_TRUE(task.has_value());
    bundle = String::replaceAll(bundle, "###PRESCRIPTIONID###", task->prescriptionId().toString());
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivate(
        task->prescriptionId(), accessCode, toCadesBesSignature(bundle, model::Timestamp::fromXsDate("2021-06-08")),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid, "BTM und Thalidomid nicht zulässig"));
}

TEST_F(A_22231_BetaebungsmittelAblehnen, category_00)//NOLINT(readability-function-cognitive-complexity)
{
    auto& resourceManager = ResourceManager::instance();
    auto bundle = resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_category_code.xml");
    bundle = String::replaceAll(bundle, "###CODE###", "00");
    bundle = String::replaceAll(bundle, "###COVERAGE###", "GKV");
    bundle = patchVersionsInBundle(bundle);
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    bundle = String::replaceAll(bundle, "###PRESCRIPTIONID###", task->prescriptionId().toString());
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivate(task->prescriptionId(), accessCode,
                                         toCadesBesSignature(bundle, model::Timestamp::fromXsDate("2021-06-08")),
                                         HttpStatus::OK));
}

TEST_F(A_22231_BetaebungsmittelAblehnen, category_01)//NOLINT(readability-function-cognitive-complexity)
{
    auto& resourceManager = ResourceManager::instance();
    auto bundle = resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_category_code.xml");
    bundle = String::replaceAll(bundle, "###CODE###", "01");
    bundle = String::replaceAll(bundle, "###COVERAGE###", "GKV");
    bundle = patchVersionsInBundle(bundle);
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    bundle = String::replaceAll(bundle, "###PRESCRIPTIONID###", task->prescriptionId().toString());
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivate(
        task->prescriptionId(), accessCode, toCadesBesSignature(bundle, model::Timestamp::fromXsDate("2021-06-08")),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid, "BTM und Thalidomid nicht zulässig"));
}

TEST_F(A_22231_BetaebungsmittelAblehnen, category_02)//NOLINT(readability-function-cognitive-complexity)
{
    auto& resourceManager = ResourceManager::instance();
    auto bundle = resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_category_code.xml");
    bundle = String::replaceAll(bundle, "###CODE###", "02");
    bundle = String::replaceAll(bundle, "###COVERAGE###", "GKV");
    bundle = patchVersionsInBundle(bundle);
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    bundle = String::replaceAll(bundle, "###PRESCRIPTIONID###", task->prescriptionId().toString());
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivate(
        task->prescriptionId(), accessCode, toCadesBesSignature(bundle, model::Timestamp::fromXsDate("2021-06-08")),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid, "BTM und Thalidomid nicht zulässig"));
}
#endif
