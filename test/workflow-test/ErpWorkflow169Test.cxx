// /*
//  * (C) Copyright IBM Deutschland GmbH 2021
//  * (C) Copyright IBM Corp. 2021
//  *

#include "erp/ErpRequirements.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/fhir/Fhir.hxx"
#include "test/util/StaticData.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include "tools/ResourceManager.hxx"

TEST_F(ErpWorkflowTest, WF169PatientAbortOnlyCompletedTasks)
{
    A_22102.test("");
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, model::PrescriptionType::direkteZuweisung));
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

TEST_F(ErpWorkflowTest, WF169PatientNoAccessCode)
{
    A_21360.test("");
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, model::PrescriptionType::direkteZuweisung));
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
