/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
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

        mKbvBundleXml = kbvBundleXml({.prescriptionId = task->prescriptionId(), .authoredOn = timestamp});
        const auto patientPos = mKbvBundleXml.find("<Patient>");
        ASSERT_NE(patientPos, std::string::npos);
        patientIdentifierBeginPos = mKbvBundleXml.find("<identifier>", patientPos);
        ASSERT_NE(patientIdentifierBeginPos, std::string::npos);
        const std::string_view patIdentifierEnd{"</identifier>"};
        patientIdentifierEndPos = mKbvBundleXml.find(patIdentifierEnd, patientIdentifierBeginPos);
        ASSERT_NE(patientIdentifierEndPos, std::string::npos);
        patientIdentifierEndPos += patIdentifierEnd.size();
    }

protected:
    std::string mKbvBundleXml;
    std::optional<model::Task> task;
    model::Timestamp timestamp = model::Timestamp::now();
    std::size_t patientIdentifierBeginPos = 0;
    std::size_t patientIdentifierEndPos = 0;
};

TEST_F(Erp6590Test, fail_bundleNoPatientIdentifier)
{
    mKbvBundleXml.erase(patientIdentifierBeginPos, patientIdentifierEndPos - patientIdentifierBeginPos);
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), task->accessCode(), toCadesBesSignature(mKbvBundleXml, timestamp),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(Erp6590Test, fail_emptyPatientIdentifier)
{
    patientIdentifierBeginPos += 13;
    patientIdentifierEndPos -= 14;
    mKbvBundleXml.erase(patientIdentifierBeginPos, patientIdentifierEndPos - patientIdentifierBeginPos);
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), task->accessCode(), toCadesBesSignature(mKbvBundleXml, timestamp),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(Erp6590Test, fail_wrongPatientIdentifierSystem)
{
    // modify the identifier system
    const auto kvid10Pos = mKbvBundleXml.find("kvid-10", patientIdentifierBeginPos);
    ASSERT_NE(kvid10Pos, std::string::npos);
    mKbvBundleXml.erase(kvid10Pos, 5);
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), task->accessCode(), toCadesBesSignature(mKbvBundleXml, timestamp),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(Erp6590Test, fail_shortPatientIdentifier)
{
    mKbvBundleXml =
        kbvBundleXml({.prescriptionId = task->prescriptionId(), .authoredOn = timestamp, .kvnr = "S04046411"});
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(
        task->prescriptionId(), task->accessCode(), toCadesBesSignature(mKbvBundleXml, timestamp),
        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}
