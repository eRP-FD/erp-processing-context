/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/fhir/Fhir.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <gtest/gtest.h>

class Erp6590Test : public ErpWorkflowTest
{
public:
    void SetUp() override
    {
        ASSERT_NO_FATAL_FAILURE(task = taskCreate());
        ASSERT_TRUE(task.has_value());

        kbvBundleXml =
            ResourceTemplates::kbvBundleXml({.prescriptionId = task->prescriptionId(), .timestamp = timestamp});
        const auto patientPos = kbvBundleXml.find("<Patient>");
        ASSERT_NE(patientPos, std::string::npos);
        patientIdentifierBeginPos = kbvBundleXml.find("<identifier>", patientPos);
        ASSERT_NE(patientIdentifierBeginPos, std::string::npos);
        const std::string_view patIdentifierEnd{"</identifier>"};
        patientIdentifierEndPos = kbvBundleXml.find(patIdentifierEnd, patientIdentifierBeginPos);
        ASSERT_NE(patientIdentifierEndPos, std::string::npos);
        patientIdentifierEndPos += patIdentifierEnd.size();
    }

protected:
    std::string kbvBundleXml;
    std::optional<model::Task> task;
    model::Timestamp timestamp = model::Timestamp::fromXsDate("2021-04-02");
    std::size_t patientIdentifierBeginPos = 0;
    std::size_t patientIdentifierEndPos = 0;
};

TEST_F(Erp6590Test, fail_bundleNoPatientIdentifier)
{
    kbvBundleXml.erase(patientIdentifierBeginPos, patientIdentifierEndPos - patientIdentifierBeginPos);
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), task->accessCode(), toCadesBesSignature(kbvBundleXml, timestamp),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(Erp6590Test, fail_emptyPatientIdentifier)
{
    patientIdentifierBeginPos += 13;
    patientIdentifierEndPos -= 14;
    kbvBundleXml.erase(patientIdentifierBeginPos, patientIdentifierEndPos - patientIdentifierBeginPos);
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), task->accessCode(), toCadesBesSignature(kbvBundleXml, timestamp),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(Erp6590Test, fail_wrongPatientIdentifierSystem)
{
    // modify the identifier system
    const auto kvid10Pos = kbvBundleXml.find("kvid-10", patientIdentifierBeginPos);
    ASSERT_NE(kvid10Pos, std::string::npos);
    kbvBundleXml.erase(kvid10Pos, 5);
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), task->accessCode(), toCadesBesSignature(kbvBundleXml, timestamp),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(Erp6590Test, fail_shortPatientIdentifier)
{
    kbvBundleXml = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = task->prescriptionId(), .timestamp = timestamp, .kvnr = "S04046411"});
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), task->accessCode(), toCadesBesSignature(kbvBundleXml, timestamp),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}
