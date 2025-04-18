/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/OperationOutcome.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"


class Erp8090Test : public ErpWorkflowTest
{
};

TEST_F(Erp8090Test, validCoverage1)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Task> task;
    const auto authoredOn = model::Timestamp::now();

    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    auto bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                                .authoredOn = authoredOn,
                                .coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis",
                                .coverageInsuranceType = "GKV"});
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), std::string{task->accessCode()},
                                                       toCadesBesSignature(bundle, authoredOn), HttpStatus::OK));

    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                           .authoredOn = authoredOn,
                           .coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis",
                           .coverageInsuranceType = "SEL"});
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), std::string{task->accessCode()},
                                                       toCadesBesSignature(bundle, authoredOn), HttpStatus::OK));

    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv));
    ASSERT_TRUE(task.has_value());
    bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                           .authoredOn = authoredOn,
                           .coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis",
                           .coverageInsuranceType = "PKV"});
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), std::string{task->accessCode()},
                                                              toCadesBesSignature(bundle, authoredOn), HttpStatus::OK));

    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::direkteZuweisungPkv));
    ASSERT_TRUE(task.has_value());
    bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                           .authoredOn = authoredOn,
                           .coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis",
                           .coverageInsuranceType = "PKV"});
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), std::string{task->accessCode()},
                                                              toCadesBesSignature(bundle, authoredOn), HttpStatus::OK));
}


TEST_F(Erp8090Test, validCoverage2)//NOLINT(readability-function-cognitive-complexity)
{
    const auto authoredOn = model::Timestamp::now();
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    const auto extensionCoverage =
        R"(<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Alternative_IK"><valueIdentifier><system value=")" +
        std::string{model::resource::naming_system::argeIknr} +
        R"(" /><value value="121191241" /></valueIdentifier></extension>)";

    auto bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                                .authoredOn = authoredOn,
                                .coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis",
                                .coverageInsuranceType = "BG",
                                .coveragePayorExtension = extensionCoverage});
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), std::string{task->accessCode()},
                                                       toCadesBesSignature(bundle, authoredOn), HttpStatus::OK));

    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    bundle = kbvBundleXml({.prescriptionId = task->prescriptionId(),
                           .authoredOn = authoredOn,
                           .coverageInsuranceSystem = "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Payor_Type_KBV",
                           .coverageInsuranceType = "UK",
                           .coveragePayorExtension = extensionCoverage});
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), std::string{task->accessCode()},
                                                              toCadesBesSignature(bundle, authoredOn), HttpStatus::OK));
}
