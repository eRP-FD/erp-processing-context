/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/OperationOutcome.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"


class A_22231_BetaebungsmittelAblehnen : public ErpWorkflowTest
{
};

TEST_F(A_22231_BetaebungsmittelAblehnen,
       category_00_direkteZuweisung)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::direkteZuweisung));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    auto bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                                .authoredOn = authoredOn,
                                .kbvVersion = kbvVersion,
                                .medicationOptions = {.version = kbvVersion, .medicationCategory = "00"}});
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode,
                                                              toCadesBesSignature(bundle, authoredOn), HttpStatus::OK));
}

TEST_F(A_22231_BetaebungsmittelAblehnen,
       category_01_direkteZuweisung)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::direkteZuweisung));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    auto bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                                .authoredOn = authoredOn,
                                .kbvVersion = kbvVersion,
                                .medicationOptions = {.version = kbvVersion, .medicationCategory = "01"}});
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), accessCode, toCadesBesSignature(bundle, authoredOn), HttpStatus::BadRequest,
        model::OperationOutcome::Issue::Type::invalid, "BTM und Thalidomid nicht zulässig"));
}

TEST_F(A_22231_BetaebungsmittelAblehnen, category_02_apothekenpflichtigeArzneimittelPkv)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    auto bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                                .authoredOn = authoredOn,
                                .kbvVersion = kbvVersion,
                                .medicationOptions = {.version = kbvVersion, .medicationCategory = "02"}});
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), accessCode, toCadesBesSignature(bundle, authoredOn), HttpStatus::BadRequest,
        model::OperationOutcome::Issue::Type::invalid, "BTM und Thalidomid nicht zulässig"));
}

TEST_F(A_22231_BetaebungsmittelAblehnen, category_00)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    auto bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                                .authoredOn = authoredOn,
                                .kbvVersion = kbvVersion,
                                .medicationOptions = {.version = kbvVersion, .medicationCategory = "00"}});
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode,
                                                              toCadesBesSignature(bundle, authoredOn), HttpStatus::OK));
}

TEST_F(A_22231_BetaebungsmittelAblehnen, category_01)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    auto bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                                .authoredOn = authoredOn,
                                .kbvVersion = kbvVersion,
                                .medicationOptions = {.version = kbvVersion, .medicationCategory = "01"}});
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), accessCode, toCadesBesSignature(bundle, authoredOn), HttpStatus::BadRequest,
        model::OperationOutcome::Issue::Type::invalid, "BTM und Thalidomid nicht zulässig"));
}

TEST_F(A_22231_BetaebungsmittelAblehnen, category_02)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    auto bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                                .authoredOn = authoredOn,
                                .kbvVersion = kbvVersion,
                                .medicationOptions = {.version = kbvVersion, .medicationCategory = "02"}});
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), accessCode, toCadesBesSignature(bundle, authoredOn), HttpStatus::BadRequest,
        model::OperationOutcome::Issue::Type::invalid, "BTM und Thalidomid nicht zulässig"));
}
