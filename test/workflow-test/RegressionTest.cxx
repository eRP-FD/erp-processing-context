/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */
#include "test/util/ResourceManager.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "test/util/TestUtils.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <gtest/gtest.h>

class RegressionTest : public ErpWorkflowTest
{
};

TEST_F(RegressionTest, Erp10674)
{
    std::string kbv_bundle_xml =
        ResourceManager::instance().getStringResource("test/validation/xml/kbv/bundle/Bundle_invalid_ERP-10674.xml");
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    kbv_bundle_xml = String::replaceAll(kbv_bundle_xml, "160.000.008.870.312.04", task->prescriptionId().toString());
    kbv_bundle_xml = patchVersionsInBundle(kbv_bundle_xml);
    std::string accessCode{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(
        taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode,
                     toCadesBesSignature(kbv_bundle_xml, model::Timestamp::fromXsDate("2022-07-29")),
                     HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(RegressionTest, Erp10669)
{
    auto timestamp = model::Timestamp::fromXsDateTime("2022-07-29T00:05:57+02:00");
    auto signingDay = model::Timestamp::fromXsDate("2022-07-29");
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    std::string kbv_bundle_xml = ResourceTemplates::kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                                     .timestamp = signingDay,
                                                                     .redeemPeriodStart = timestamp,
                                                                     .redeemPeriodEnd = {}});
    std::string accessCode{task->accessCode()};
    std::optional<model::Task> taskActivateResult;
    ASSERT_NO_FATAL_FAILURE(taskActivateResult = taskActivateWithOutcomeValidation(
                                task->prescriptionId(), accessCode, toCadesBesSignature(kbv_bundle_xml, signingDay)));
    ASSERT_TRUE(taskActivateResult);
    EXPECT_EQ(taskActivateResult->expiryDate().toGermanDate(), "2023-07-29");
    EXPECT_EQ(taskActivateResult->acceptDate().toGermanDate(), "2023-07-29");
    auto bundle = taskGetId(taskActivateResult->prescriptionId(), taskActivateResult->kvnr().value().id());
    ASSERT_TRUE(bundle);
    std::optional<model::Task> getTaskResult;
    getTaskFromBundle(getTaskResult, *bundle);
    ASSERT_TRUE(getTaskResult);
    EXPECT_EQ(getTaskResult->expiryDate().toGermanDate(), "2023-07-29");
    EXPECT_EQ(getTaskResult->acceptDate().toGermanDate(), "2023-07-29");
}

TEST_F(RegressionTest, Erp10835)
{
    auto profileEnv = testutils::getNewFhirProfileEnvironment();
    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};
    auto task = taskCreate(model::PrescriptionType::direkteZuweisungPkv);
    ASSERT_TRUE(task.has_value());
    auto kvnr = generateNewRandomKVNR().id();
    ASSERT_NO_FATAL_FAILURE(
        taskActivateWithOutcomeValidation(task->prescriptionId(), task->accessCode(),
                     std::get<0>(makeQESBundle(kvnr, task->prescriptionId(), model::Timestamp::now()))));
    ASSERT_NO_FATAL_FAILURE(consentPost(kvnr, model::Timestamp::now()));
    const auto acceptBundle = taskAccept(task->prescriptionId(), std::string{task->accessCode()});
    ASSERT_TRUE(acceptBundle);
    const auto acceptedTasks = acceptBundle->getResourcesByType<model::Task>();
    ASSERT_EQ(acceptedTasks.size(), 1);
    ASSERT_NO_FATAL_FAILURE(
        taskClose(task->prescriptionId(), std::string{acceptedTasks[0].secret().value_or("")}, kvnr));
    ASSERT_NO_FATAL_FAILURE(chargeItemPost(task->prescriptionId(), kvnr, "3-SMC-B-Testkarte-883110000120312",
                                           std::string{acceptedTasks[0].secret().value_or("")}));
}

class RegressionTestErp8170 : public ErpWorkflowTest
{
protected:
    std::string medicationDispense(const std::string& kvnr, const std::string& prescriptionIdForMedicationDispense,
                                   const std::string&, model::ResourceVersion::FhirProfileBundleVersion) override
    {
        // TODO: adjust for version 1.1.0
        auto medicationDispense =
            ResourceManager::instance().getStringResource("test/issues/ERP-8170/MedicationDispense.xml");
        medicationDispense =
            String::replaceAll(medicationDispense, "160.000.074.296.321.22", prescriptionIdForMedicationDispense);
        medicationDispense = String::replaceAll(medicationDispense, "X110455449", kvnr);
        medicationDispense = String::replaceAll(medicationDispense, "3-SMC-B-Testkarte-883110000116298",
                                                jwtApotheke().stringForClaim(JWT::idNumberClaim).value());
        return medicationDispense;
    }
};

TEST_F(RegressionTestErp8170, Erp8170)
{
    auto task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel);
    ASSERT_TRUE(task.has_value());
    auto kvnr = generateNewRandomKVNR().id();
    ASSERT_NO_FATAL_FAILURE(
        taskActivateWithOutcomeValidation(task->prescriptionId(), task->accessCode(),
                     std::get<0>(makeQESBundle(kvnr, task->prescriptionId(), model::Timestamp::now()))));
    const auto acceptBundle = taskAccept(task->prescriptionId(), std::string{task->accessCode()});
    ASSERT_TRUE(acceptBundle);
    const auto acceptedTasks = acceptBundle->getResourcesByType<model::Task>();
    ASSERT_EQ(acceptedTasks.size(), 1);
    ASSERT_NO_FATAL_FAILURE(
        taskClose(task->prescriptionId(), std::string{acceptedTasks[0].secret().value_or("")}, kvnr));
}

TEST_F(RegressionTest, Erp11116)
{
    auto task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel);
    ASSERT_TRUE(task.has_value());
    auto accessCode = task->accessCode();

    std::string kbv_bundle_xml = ResourceManager::instance().getStringResource(
        "test/issues/ERP-11116/bundle.xml");
    kbv_bundle_xml = String::replaceAll(kbv_bundle_xml, "160.000.000.009.652.07", task->prescriptionId().toString());
    kbv_bundle_xml = patchVersionsInBundle(kbv_bundle_xml);

    ASSERT_NO_FATAL_FAILURE(
        taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode,
                     toCadesBesSignature(kbv_bundle_xml, model::Timestamp::fromXsDateTime("2022-09-14T00:05:57+02:00")),
                     HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(RegressionTest, Erp11142)
{
    auto task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel);
    ASSERT_TRUE(task.has_value());
    auto accessCode = task->accessCode();

    std::string kbv_bundle_xml = ResourceManager::instance().getStringResource(
        "test/issues/ERP-11142/Bundle_invalid_missing_meta_profile_version.xml");
    kbv_bundle_xml = String::replaceAll(kbv_bundle_xml, "160.000.000.012.230.33", task->prescriptionId().toString());
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), accessCode,
        toCadesBesSignature(kbv_bundle_xml, model::Timestamp::fromXsDateTime("2022-09-14T00:05:57+02:00")),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
        "KBV resources must always define a profile version"));

    // additional test with duplicate version, KBV_PR_ERP_Bundle|1.0.3|1.0.3
    kbv_bundle_xml = String::replaceAll(
        kbv_bundle_xml, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle",
        "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|" +
            std::string(v_str(model::ResourceVersion::current<model::ResourceVersion::KbvItaErp>())) + "|" +
            std::string(v_str(model::ResourceVersion::current<model::ResourceVersion::KbvItaErp>())));
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), accessCode,
        toCadesBesSignature(kbv_bundle_xml, model::Timestamp::fromXsDateTime("2022-09-14T00:05:57+02:00")),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
        "Invalid meta/profile: more than one | separator found."));
}

TEST_F(RegressionTest, Erp10892_1)
{
    std::string kbv_bundle_xml =
        ResourceManager::instance().getStringResource("test/issues/ERP-10892/meta_profile_array_size.xml");
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    kbv_bundle_xml = String::replaceAll(kbv_bundle_xml, "160.000.000.012.588.26", task->prescriptionId().toString());
    std::string accessCode{task->accessCode()};
    std::optional<model::Task> taskActivateResult;
    ASSERT_NO_FATAL_FAILURE(
        taskActivateResult = taskActivateWithOutcomeValidation(
            task->prescriptionId(), accessCode,
            toCadesBesSignature(kbv_bundle_xml, model::Timestamp::fromXsDateTime("2022-07-29T00:05:57+02:00")),
            HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(RegressionTest, Erp10892_2)
{
    std::string kbv_bundle_xml =
        ResourceManager::instance().getStringResource("test/issues/ERP-10892/negative_timestamp.xml");
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    kbv_bundle_xml = String::replaceAll(kbv_bundle_xml, "160.000.000.012.588.26", task->prescriptionId().toString());
    std::string accessCode{task->accessCode()};
    std::optional<model::Task> taskActivateResult;
    ASSERT_NO_FATAL_FAILURE(
        taskActivateResult = taskActivateWithOutcomeValidation(
            task->prescriptionId(), accessCode,
            toCadesBesSignature(kbv_bundle_xml, model::Timestamp::fromXsDateTime("2022-07-29T00:05:57+02:00")),
            HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(RegressionTest, Erp11050)
{
    std::string kbv_bundle_400_xml =
        ResourceManager::instance().getStringResource("test/issues/ERP-11050/activate_400.xml");
    std::string kbv_bundle_500_xml =
        ResourceManager::instance().getStringResource("test/issues/ERP-11050/activate_500.xml");
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    kbv_bundle_400_xml = String::replaceAll(kbv_bundle_400_xml, "160.000.000.040.283.70", task->prescriptionId().toString());
    kbv_bundle_500_xml = String::replaceAll(kbv_bundle_500_xml, "160.000.000.040.284.67", task->prescriptionId().toString());
    std::string accessCode{task->accessCode()};
    std::optional<model::Task> taskActivateResult;
    ASSERT_NO_FATAL_FAILURE(
        taskActivateResult = taskActivateWithOutcomeValidation(
            task->prescriptionId(), accessCode,
            toCadesBesSignature(kbv_bundle_400_xml, model::Timestamp::fromXsDateTime("2022-07-29T00:05:57+02:00")),
            HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
    ASSERT_NO_FATAL_FAILURE(
        taskActivateResult = taskActivateWithOutcomeValidation(
            task->prescriptionId(), accessCode,
            toCadesBesSignature(kbv_bundle_500_xml, model::Timestamp::fromXsDateTime("2022-07-29T00:05:57+02:00")),
            HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}
