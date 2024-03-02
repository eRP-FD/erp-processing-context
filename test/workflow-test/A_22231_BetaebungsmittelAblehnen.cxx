/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/OperationOutcome.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "test/util/ResourceTemplates.hxx"


class A_22231_BetaebungsmittelAblehnen : public ErpWorkflowTest
{
};

TEST_F(A_22231_BetaebungsmittelAblehnen, category_00_direkteZuweisung)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::direkteZuweisung));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto bundle =
        kbvBundleXml({.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn, .medicationCategory = "00"});
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode,
                                         toCadesBesSignature(bundle, authoredOn),
                                         HttpStatus::OK));
}

TEST_F(A_22231_BetaebungsmittelAblehnen, category_01_direkteZuweisung)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::direkteZuweisung));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(), .medicationCategory = "01"});
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), accessCode, toCadesBesSignature(bundle, authoredOn),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid, "BTM und Thalidomid nicht zulässig"));
}

TEST_F(A_22231_BetaebungsmittelAblehnen, category_02_apothekenpflichtigeArzneimittelPkv)
{
    if (serverUsesOldProfile())
    {
        GTEST_SKIP_("PKV not testable with old profiles");
    }
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto bundle =
        kbvBundleXml({.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn, .medicationCategory = "02"});
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), accessCode, toCadesBesSignature(bundle, authoredOn),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid, "BTM und Thalidomid nicht zulässig"));
}

TEST_F(A_22231_BetaebungsmittelAblehnen, category_00)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto bundle =
        kbvBundleXml({.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn, .medicationCategory = "00"});
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode,
                                         toCadesBesSignature(bundle, authoredOn),
                                         HttpStatus::OK));
}

TEST_F(A_22231_BetaebungsmittelAblehnen, category_01)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto bundle =
        kbvBundleXml({.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn, .medicationCategory = "01"});
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), accessCode, toCadesBesSignature(bundle, authoredOn),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid, "BTM und Thalidomid nicht zulässig"));
}

TEST_F(A_22231_BetaebungsmittelAblehnen, category_02)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto bundle =
        kbvBundleXml({.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn, .medicationCategory = "02"});
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), accessCode, toCadesBesSignature(bundle, authoredOn),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid, "BTM und Thalidomid nicht zulässig"));
}
