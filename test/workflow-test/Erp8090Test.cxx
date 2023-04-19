/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/OperationOutcome.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"


class Erp8090Test : public ErpWorkflowTest
{
};

TEST_F(Erp8090Test, validCoverage1)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Task> task;
    const auto timestamp = model::Timestamp::fromXsDate("2021-06-08");

    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    auto bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                                .timestamp = timestamp,
                                .coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis",
                                .coverageInsuranceType = "GKV"});
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), std::string{task->accessCode()},
                                                       toCadesBesSignature(bundle, timestamp), HttpStatus::OK));

    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                           .timestamp = timestamp,
                           .coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis",
                           .coverageInsuranceType = "SEL"});
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), std::string{task->accessCode()},
                                                       toCadesBesSignature(bundle, timestamp), HttpStatus::OK));

    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

    if (! serverUsesOldProfile())
    {
        ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv));
        ASSERT_TRUE(task.has_value());
        bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                               .timestamp = timestamp,
                               .coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis",
                               .coverageInsuranceType = "PKV"});
        ASSERT_NO_FATAL_FAILURE(
            taskActivateWithOutcomeValidation(task->prescriptionId(), std::string{task->accessCode()},
                                              toCadesBesSignature(bundle, timestamp), HttpStatus::OK));

        ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::direkteZuweisungPkv));
        ASSERT_TRUE(task.has_value());
        bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                               .timestamp = timestamp,
                               .coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis",
                               .coverageInsuranceType = "PKV"});
        ASSERT_NO_FATAL_FAILURE(
            taskActivateWithOutcomeValidation(task->prescriptionId(), std::string{task->accessCode()},
                                              toCadesBesSignature(bundle, timestamp), HttpStatus::OK));
    }
}


TEST_F(Erp8090Test, validCoverage2)//NOLINT(readability-function-cognitive-complexity)
{
    const auto timestamp = model::Timestamp::fromXsDate("2021-06-08");
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    auto const nsArgeIknr =
        model::ResourceVersion::currentBundle() == model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01
            ? model::resource::naming_system::deprecated::argeIknr
            : model::resource::naming_system::argeIknr;
    const auto extensionCoverage =
        R"(<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Alternative_IK"><valueIdentifier><system value=")" +
        std::string{nsArgeIknr} + R"(" /><value value="121191241" /></valueIdentifier></extension>)";

    auto bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                                .timestamp = timestamp,
                                .coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis",
                                .coverageInsuranceType = "BG",
                                .coveragePayorExtension = extensionCoverage});
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), std::string{task->accessCode()},
                                                       toCadesBesSignature(bundle, timestamp), HttpStatus::OK));

    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                           .timestamp = timestamp,
                           .coverageInsuranceSystem = "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Payor_Type_KBV",
                           .coverageInsuranceType = "UK",
                           .coveragePayorExtension = extensionCoverage});
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), std::string{task->accessCode()},
        toCadesBesSignature(bundle, model::Timestamp::fromXsDate("2021-06-08")), HttpStatus::OK));
}

TEST_F(Erp8090Test, invalidCoverage)//NOLINT(readability-function-cognitive-complexity)
{
    const auto isDeprecated =
        model::ResourceVersion::currentBundle() == model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01;
    if (! isDeprecated)
    {
        GTEST_SKIP_("Disable for new profiles, unable to test for particular failures");
    }

    const auto timestamp = model::Timestamp::fromXsDate("2021-06-08");
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());

    auto bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                                .timestamp = timestamp,
                                .coverageInsuranceSystem = "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Payor_Type_KBV",
                                .coverageInsuranceType = "SKT"});
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), std::string{task->accessCode()}, toCadesBesSignature(bundle, timestamp),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid, "Kostenträger nicht zulässig"));

    // with the new profiles we'd have to pass PrescriptionType::apothekenpflichtigeArzneimittelPkv to
    // taskCreate, otherwise the profile validator will keep us out, but then we have to enable
    // the PKV feature. So skip it in this case

    EnvironmentVariableGuard disablePkv{ConfigurationKey::FEATURE_PKV, "false"};

    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                           .timestamp = timestamp,
                           .coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis",
                           .coverageInsuranceType = "PKV"});
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), std::string{task->accessCode()}, toCadesBesSignature(bundle, timestamp),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid, "Kostenträger nicht zulässig"));
}
