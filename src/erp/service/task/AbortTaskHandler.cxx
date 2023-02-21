/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/task/AbortTaskHandler.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/util/TLog.hxx"


void AbortTaskHandler::checkAccessValidityPharmacy(
        const model::Task &task,
        const ServerRequest &request)
{
    A_19145.start("Check correct task status for user pharmacy");
    ErpExpect(task.status() == model::Task::Status::inprogress, HttpStatus::Forbidden,
              "Task must be in progress for user pharmacy, is: " +
                  std::string(model::Task::StatusNames.at(task.status())));
    A_19145.finish();

    // check secret
    A_19224.start("Check secret for user pharmacy");
    A_20703.start("Set VAU-Error-Code header field to brute-force whenever AccessCode or Secret mismatches");
    const auto uriSecret = request.getQueryParameter("secret");
    VauExpect(uriSecret.has_value() && uriSecret.value() == task.secret(), HttpStatus::Forbidden,
              VauErrorCode::brute_force, "No or invalid secret provided for user pharmacy");
    A_20703.finish();
    A_19224.finish();
}


void AbortTaskHandler::checkAccessValidityOutsidePharmacy(
    AuditDataCollector& auditDataCollector,
    const std::string& professionOIDClaim,
    const model::Task &task,
    const ServerRequest &request)
{
    A_19146.start("Check correct task status for user other than pharmacy");
    ErpExpect(task.status() != model::Task::Status::inprogress, HttpStatus::Forbidden,
              "Task must not be in progress for users other than pharmacy, but is: " +
                  std::string(model::Task::StatusNames.at(task.status())));
    A_19146.finish();

    if(professionOIDClaim == profession_oid::oid_versicherter)
    {
        const auto kvNrClaim = request.getAccessToken().stringForClaim(JWT::idNumberClaim);
        const auto kvNrFromTask = task.kvnr();
        Expect3(kvNrFromTask.has_value(), "Task has no KV number", std::logic_error);
        A_20546_01.start("Check insurance number for insured person (patient)");
        if (kvNrClaim == kvNrFromTask)
        {
            auditDataCollector.setEventId(model::AuditEventId::POST_Task_abort_insurant);
            return;
        }
        A_20546_01.finish();
    }
    else // For oid_arzt, oid_zahnarzt, oid_praxis_arzt, oid_zahnarztpraxis, oid_praxis_psychotherapeut, oid_krankenhaus
    {
        A_19120_3.start("Check correct task status for doctor");
        ErpExpect(task.status() == model::Task::Status::ready, HttpStatus::Forbidden,
              "Task must be ready for doctor, but is: " +
                  std::string(model::Task::StatusNames.at(task.status())));
        A_19120_3.finish();
    }

    A_20547.start("Check access code for insured person kvnr mismatch (representative)");
    A_19120_3.start("Check access code for prescriptive authority");
    checkAccessCodeMatches(request, task);
    auditDataCollector.setEventId(professionOIDClaim == profession_oid::oid_versicherter
                                  ? model::AuditEventId::POST_Task_abort_representative
                                  : model::AuditEventId::POST_Task_abort_doctor);
    A_20547.finish();
    A_19120_3.finish();
}


void AbortTaskHandler::checkAccessValidity(
    AuditDataCollector& auditDataCollector,
    const std::string& professionOIDClaim,
    const model::Task &task,
    const ServerRequest &request)
{
    if(professionOIDClaim == profession_oid::oid_oeffentliche_apotheke ||
       professionOIDClaim == profession_oid::oid_krankenhausapotheke)
    {
        auditDataCollector.setEventId(model::AuditEventId::POST_Task_abort_pharmacy);
        checkAccessValidityPharmacy(task, request);
    }
    else
    {
        checkAccessValidityOutsidePharmacy(auditDataCollector, professionOIDClaim, task, request);
    }
}


AbortTaskHandler::AbortTaskHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : TaskHandlerBase(Operation::POST_Task_id_abort, allowedProfessionOiDs)
{
}

void AbortTaskHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto prescriptionId = parseId(session.request, session.accessLog);
    checkFeatureWf200(prescriptionId.type());
    TVLOG(1) << "Working on Task for prescription id " << prescriptionId.toString();

    const auto professionOIDClaim = session.request.getAccessToken().stringForClaim(JWT::professionOIDClaim);
    Expect3(professionOIDClaim.has_value(), "Missing professionOIDClaim", std::logic_error);  // should not happen because of professionOID check;

    auto* databaseHandle = session.database();
    auto task = databaseHandle->retrieveTaskForUpdate(prescriptionId);

    ErpExpect(task.has_value(), HttpStatus::NotFound, "Task not found for prescription id");

    const auto taskStatus = task->status();
    ErpExpect(taskStatus != model::Task::Status::cancelled, HttpStatus::Gone, "Task has already been deleted");
    ErpExpect(taskStatus != model::Task::Status::draft, HttpStatus::Forbidden, "Abort not expected for newly created Task");

    A_22102_01.start("insurant not allowed to delete incomplete tasks for workflow types 169 and 209.");
    if(professionOIDClaim.value() == profession_oid::oid_versicherter)
    {
        switch (task->type())
        {
            case model::PrescriptionType::apothekenpflichigeArzneimittel:
            case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
                break;
            case model::PrescriptionType::direkteZuweisung:
            case model::PrescriptionType::direkteZuweisungPkv:
                ErpExpect(taskStatus == model::Task::Status::completed, HttpStatus::Forbidden,
                          "Abort for patient in workflow types 169 / 209 only allowed for completed Task");
                break;
        }
    }
    A_22102_01.finish();

    checkAccessValidity(session.auditDataCollector(), *professionOIDClaim, *task, session.request);

    const auto kvnr = task->kvnr();
    Expect3(kvnr.has_value(), "Task has no KV number", std::logic_error);

    A_19121.start("Set Task status to cancelled");
    task->setStatus(model::Task::Status::cancelled);
    A_19121.finish();

    A_19027_02.start("Delete personal data");
    // Task.for (KVNR of patient)
    task->deleteKvnr();
    // Task.identifier (AccessCode)
    task->deleteAccessCode();
    // Task.identifier (Secret)
    task->deleteSecret();
    // Task related Communications
    databaseHandle->deleteCommunicationsForTask(task->prescriptionId());
    task->updateLastUpdate();

    // Update task in database and delete related HealthCareProviderPrescription, PatientConfirmation,
    // Receipt, MedicationDispense:
    databaseHandle->updateTaskClearPersonalData(*task);
    A_19027_02.finish();

    A_19514.start("HttpStatus 204 for successful POST");
    makeResponse(session, HttpStatus::NoContent, nullptr/*body*/);
    A_19514.finish();

    // Collect Audit data
    session.auditDataCollector()
        .setInsurantKvnr(*kvnr)
        .setPrescriptionId(prescriptionId)
        .setAction(model::AuditEvent::Action::del);
}
