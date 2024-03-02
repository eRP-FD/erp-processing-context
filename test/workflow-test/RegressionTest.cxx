/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "test/util/ResourceManager.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "test/util/TestUtils.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <date/tz.h>
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
                     toCadesBesSignature(kbv_bundle_xml, model::Timestamp::fromXsDate("2022-07-29", model::Timestamp::UTCTimezone)),
                     HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(RegressionTest, Erp10669)
{
    using namespace std::literals::chrono_literals;
    using zoned_ms = date::zoned_time<std::chrono::milliseconds>;
    // today at 00:05 in german time zone
    auto zt = zoned_ms{model::Timestamp::GermanTimezone, model::Timestamp::now().localDay() + 5min};
    auto authoredOn = model::Timestamp(zt.get_sys_time());
    auto signingDay = model::Timestamp::now();

    ASSERT_EQ(signingDay.toGermanDate(), authoredOn.toGermanDate()); // sanity check to ensure this is the same day
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    std::string kbv_bundle_xml = kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                  .authoredOn = signingDay,
                                                  .redeemPeriodStart = authoredOn.toGermanDate(),
                                                  .redeemPeriodEnd = {}});
    std::string accessCode{task->accessCode()};
    std::optional<model::Task> taskActivateResult;
    ASSERT_NO_FATAL_FAILURE(taskActivateResult = taskActivateWithOutcomeValidation(
                                task->prescriptionId(), accessCode, toCadesBesSignature(kbv_bundle_xml, signingDay)));
    ASSERT_TRUE(taskActivateResult);
    EXPECT_EQ(taskActivateResult->expiryDate().localDay(), signingDay.localDay() + date::days{365});
    EXPECT_EQ(taskActivateResult->acceptDate().localDay(), signingDay.localDay() + date::days{365});
    auto bundle = taskGetId(taskActivateResult->prescriptionId(), taskActivateResult->kvnr().value().id());
    ASSERT_TRUE(bundle);
    std::optional<model::Task> getTaskResult;
    getTaskFromBundle(getTaskResult, *bundle);
    ASSERT_TRUE(getTaskResult);
    EXPECT_EQ(getTaskResult->expiryDate().localDay(), signingDay.localDay() + date::days{365});
    EXPECT_EQ(getTaskResult->acceptDate().localDay(), signingDay.localDay() + date::days{365});
}

TEST_F(RegressionTest, Erp10835)
{
    if (! model::ResourceVersion::isProfileSupported(
            model::ResourceVersion::DeGematikErezeptPatientenrechnungR4::v1_0_0))
    {
        GTEST_SKIP();
    }
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
                                   const std::string&, model::ResourceVersion::FhirProfileBundleVersion version) override
    {
        auto gematikVersion = std::get<model::ResourceVersion::DeGematikErezeptWorkflowR4>(
            model::ResourceVersion::profileVersionFromBundle(version));
        return ResourceTemplates::medicationDispenseXml(
            {.prescriptionId = prescriptionIdForMedicationDispense,
             .kvnr = kvnr,
             .whenPrepared = model::Timestamp::fromXsDateTime("0001-01-01T00:00:00Z"),
             .gematikVersion = gematikVersion});
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
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid, "unknown or unexpected profile"));

    // additional test with duplicate version, KBV_PR_ERP_Bundle|1.0.3|1.0.3
    kbv_bundle_xml = String::replaceAll(
        kbv_bundle_xml, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle",
        "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|" +
            std::string(v_str(model::ResourceVersion::current<model::ResourceVersion::KbvItaErp>())) + "|" +
            std::string(v_str(model::ResourceVersion::current<model::ResourceVersion::KbvItaErp>())));
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), accessCode,
        toCadesBesSignature(kbv_bundle_xml, model::Timestamp::fromXsDateTime("2022-09-14T00:05:57+02:00")),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid, "unknown or unexpected profile"));
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

TEST_F(RegressionTest, Erp16393)
{
    EnvironmentVariableGuard envGuard(ConfigurationKey::FHIR_PROFILE_VALID_FROM, "2023-09-29");
    auto kbvBundleXml = ResourceManager::instance().getStringResource(
        "test/validation/xml/v_2023_07_01/kbv/bundle/Bundle_invalid_ERP-16393_ABC.xml");
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    kbvBundleXml = String::replaceAll(kbvBundleXml, "160.000.006.388.698.96", task->prescriptionId().toString());
    std::string accessCode{task->accessCode()};
    std::optional<model::Task> taskActivateResult;
    ASSERT_NO_FATAL_FAILURE(
        taskActivateResult = taskActivateWithOutcomeValidation(
            task->prescriptionId(), accessCode,
            toCadesBesSignature(kbvBundleXml, model::Timestamp::fromXsDateTime("2023-09-29T08:44:35.864+02:00")),
            HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}
