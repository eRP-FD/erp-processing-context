/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/ErpRequirements.hxx"
#include "shared/model/OperationOutcome.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <boost/algorithm/string/replace.hpp>


class A_22231_A_27813_BtmThalidomidTest : public ErpWorkflowTest
{
    static constexpr auto tests =
        A_22231_01.tests("E-Rezept-Fachdienst - Task aktivieren - Ausschluss Betäubungsmittel");
    static constexpr auto alsoTests =
        A_27813.tests("E-Rezept-Fachdienst - Task aktivieren - Flowtype 166 - Prüfung Arzneimittelverordnung");
    static constexpr auto alsoTests2 = A_25642.tests(
        "E-Rezept-Fachdienst - Task aktivieren - Flowtype 160/169/200/209 - Prüfung Arzneimittelverordnung");

protected:
    void SetUp() override
    {
        if (ResourceTemplates::Versions::KBV_ERP_current() < ResourceTemplates::Versions::KBV_ERP_1_4_2)
        {
            GTEST_SKIP() << "test disabled for KBV < " << ResourceTemplates::Versions::KBV_ERP_1_4_2;
        }
    }
};

TEST_F(A_22231_A_27813_BtmThalidomidTest, category_00_direkteZuweisung)
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

TEST_F(A_22231_A_27813_BtmThalidomidTest, category_00_tRezept)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::tRezept));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    auto fakeId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 12344);
    auto bundle = kbvBundleXml({.prescriptionId = fakeId,
                                .authoredOn = authoredOn,
                                .kbvVersion = kbvVersion,
                                .medicationOptions = {.version = kbvVersion, .medicationCategory = "00"}});
    boost::replace_all(bundle, fakeId.toString(), task->prescriptionId().toString());
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(
        taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode, toCadesBesSignature(bundle, authoredOn),
                                          HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                                          "Für diesen Workflowtypen sind nur T-Rezept Verordnungen zulässig"));
}

TEST_F(A_22231_A_27813_BtmThalidomidTest, category_01_direkteZuweisung)
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
        model::OperationOutcome::Issue::Type::invalid, "BTM nicht zulässig"));
}

TEST_F(A_22231_A_27813_BtmThalidomidTest, category_01_tRezept)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::tRezept));
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
        model::OperationOutcome::Issue::Type::invalid, fmt::format("FHIR-Validation error",
        "Bundle: error: -erp-angabeReichdauerBtM-RezeptEinheit: Sofern die Reicherdauer bei einem BtM-Rezept angegeben "
        "wird, muss diese in Tagen angegeben werden. (from profile: "
        "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|{}); Bundle: error: "
        "-erp-angabeT-RezeptAngabenVerbot: Wenn es sich nicht um eine Verordnung von T-Arzneimitteln handelt, dürfen "
        "T-Rezept-Angaben nicht vorhanden sein. (from profile: "
        "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|{}); ", ResourceTemplates::Versions::KBV_ERP_1_4_2)));
}


TEST_F(A_22231_A_27813_BtmThalidomidTest, category_02_apothekenpflichtigeArzneimittelPkv)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    auto fakeId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::tRezept, 12344);
    auto bundle = kbvBundleXml({.prescriptionId = fakeId,
                                .authoredOn = authoredOn,
                                .kbvVersion = kbvVersion,
                                .medicationOptions = {.version = kbvVersion, .medicationCategory = "02"}});
    boost::replace_all(bundle, fakeId.toString(), task->prescriptionId().toString());
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(
        taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode, toCadesBesSignature(bundle, authoredOn),
                                          HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                                          "Für diesen Workflowtypen sind nur Arzneimittelverordnungen zulässig"));
}

TEST_F(A_22231_A_27813_BtmThalidomidTest, category_02_tRezept)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::tRezept));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    auto bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                                .authoredOn = authoredOn,
                                .kbvVersion = kbvVersion,
                                .medicationOptions = {.version = kbvVersion, .medicationCategory = "02"}});
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode,
                                                              toCadesBesSignature(bundle, authoredOn), HttpStatus::OK));
}

TEST_F(A_22231_A_27813_BtmThalidomidTest, category_00_apothekenpflichigeArzneimittel)
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

TEST_F(A_22231_A_27813_BtmThalidomidTest, category_01_apothekenpflichigeArzneimittel)
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
        model::OperationOutcome::Issue::Type::invalid, "BTM nicht zulässig"));
}

TEST_F(A_22231_A_27813_BtmThalidomidTest, category_02_apothekenpflichigeArzneimittel)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    auto fakeId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::tRezept, 12344);
    auto bundle = kbvBundleXml({.prescriptionId = fakeId,
                                .authoredOn = authoredOn,
                                .kbvVersion = kbvVersion,
                                .medicationOptions = {.version = kbvVersion, .medicationCategory = "02"}});
    boost::replace_all(bundle, fakeId.toString(), task->prescriptionId().toString());
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(
        taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode, toCadesBesSignature(bundle, authoredOn),
                                          HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                                          "Für diesen Workflowtypen sind nur Arzneimittelverordnungen zulässig"));
}

TEST_F(A_22231_A_27813_BtmThalidomidTest, category_03_apothekenpflichigeArzneimittel)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    auto bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                                .authoredOn = authoredOn,
                                .kbvVersion = kbvVersion,
                                .medicationOptions = {.version = kbvVersion, .medicationCategory = "03"}});
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), accessCode, toCadesBesSignature(bundle, authoredOn), HttpStatus::BadRequest,
        model::OperationOutcome::Issue::Type::invalid, "FHIR-Validation error"));
}

TEST_F(A_22231_A_27813_BtmThalidomidTest, category_03_tRezept)
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::tRezept));
    ASSERT_TRUE(task.has_value());
    auto authoredOn = model::Timestamp::now();
    auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    auto bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                                .authoredOn = authoredOn,
                                .kbvVersion = kbvVersion,
                                .medicationOptions = {.version = kbvVersion, .medicationCategory = "03"}});
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), accessCode, toCadesBesSignature(bundle, authoredOn), HttpStatus::BadRequest,
        model::OperationOutcome::Issue::Type::invalid, "FHIR-Validation error"));
}