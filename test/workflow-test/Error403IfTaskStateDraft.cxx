/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>


class Error403IfTaskStateDraft : public ErpWorkflowTestP
{
};

TEST_P(Error403IfTaskStateDraft, run)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));

    const std::string kvnr{"X987654326"};

    // retrieve activated Task
    ASSERT_NO_FATAL_FAILURE(taskGetId(*prescriptionId, kvnr, accessCode, HttpStatus::Forbidden,
                                      model::OperationOutcome::Issue::Type::forbidden));
}

INSTANTIATE_TEST_SUITE_P(Error403IfTaskStateDraftInst, Error403IfTaskStateDraft,
                         testing::Values(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
                                         model::PrescriptionType::direkteZuweisungPkv));
