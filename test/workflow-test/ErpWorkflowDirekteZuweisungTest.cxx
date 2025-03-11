/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/eventprocessing/Outcome.hxx"
#include "shared/ErpRequirements.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"



class ErpWorkflowDirekteZuweisungTestP : public ErpWorkflowTestP
{
protected:
    void SetUp() override
    {
        ErpWorkflowTestP::SetUp();
    }
};

TEST_P(ErpWorkflowDirekteZuweisungTestP, PatientAbortOnlyCompletedTasks)//NOLINT(readability-function-cognitive-complexity)
{
    A_22102_01.test("insurant not allowed to delete incomplete tasks for flow types 169 and 209");
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));
    ASSERT_TRUE(prescriptionId.has_value());

    std::string qesBundle;
    std::vector<model::Communication> communications;
    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, prescriptionId.value(), kvnr, accessCode));

    const auto jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);

    // Abort is forbidden:
    ASSERT_NO_FATAL_FAILURE(taskAbort(*prescriptionId, jwt, accessCode, {}, HttpStatus::Forbidden,
                                      model::OperationOutcome::Issue::Type::forbidden));

    std::string secret;
    std::optional<model::Timestamp> lastModifiedDate;
    ASSERT_NO_FATAL_FAILURE(
        checkTaskAccept(secret, lastModifiedDate, prescriptionId.value(), kvnr, accessCode, qesBundle));

    // Abort is forbidden:
    ASSERT_NO_FATAL_FAILURE(taskAbort(*prescriptionId, jwt, accessCode, {}, HttpStatus::Forbidden,
                                      model::OperationOutcome::Issue::Type::forbidden));

    ASSERT_NO_FATAL_FAILURE(
        checkTaskClose(prescriptionId.value(), kvnr, secret, lastModifiedDate.value(), communications));

    // Abort is allowed for completed tasks
    ASSERT_NO_FATAL_FAILURE(checkTaskAbort(prescriptionId.value(), jwt, kvnr, accessCode, secret, {}));
}

TEST_P(ErpWorkflowDirekteZuweisungTestP, TaskRetrievalProhibitedForProxyProhibited)//NOLINT(readability-function-cognitive-complexity)
{
    A_26148.test("Representative may not retrieve Task with Flowtype 169/209");
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));
    ASSERT_TRUE(prescriptionId.has_value());

    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    std::string qesBundle;
    ASSERT_NO_THROW(qesBundle = std::get<0>(makeQESBundle(kvnr, *prescriptionId, model::Timestamp::now())));

    ASSERT_NO_FATAL_FAILURE(taskActivate(*prescriptionId,accessCode, qesBundle));

    std::optional<model::Bundle> taskBundle;
    std::string kvnrRepresentative;
    generateNewRandomKVNR(kvnrRepresentative);
    ASSERT_NE(kvnr, kvnrRepresentative);
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskGetId(prescriptionId.value(), kvnrRepresentative, accessCode, std::nullopt, HttpStatus::Forbidden, model::OperationOutcome::httpCodeToOutcomeIssueType(HttpStatus::Forbidden)));
    ASSERT_FALSE(taskBundle.has_value());
}


// GEMREQ-start A_21360-01
TEST_P(ErpWorkflowDirekteZuweisungTestP, PatientNoAccessCode)//NOLINT(readability-function-cognitive-complexity)
{
    A_21360_01.test("remove access code from task if workflow type is 169 or 209");
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));
    ASSERT_TRUE(prescriptionId.has_value());

    std::string qesBundle;
    std::vector<model::Communication> communications;
    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, prescriptionId.value(), kvnr, accessCode));

    std::optional<model::Bundle> taskBundle;
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskGetId(prescriptionId.value(), kvnr, accessCode));
    ASSERT_TRUE(taskBundle.has_value());
    auto tasks = taskBundle->getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);

    // task must not contain the access code
    ASSERT_THROW((void)tasks.at(0).accessCode(), model::ModelException);
}
// GEMREQ-end A_21360-01

INSTANTIATE_TEST_SUITE_P(ErpWorkflowDirekteZuweisungTestPInst, ErpWorkflowDirekteZuweisungTestP,
                         testing::Values(model::PrescriptionType::direkteZuweisung,
                                         model::PrescriptionType::direkteZuweisungPkv));
