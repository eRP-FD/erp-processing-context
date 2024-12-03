/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/task/RejectTaskHandler.hxx"

#include "shared/ErpRequirements.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/Task.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "shared/util/TLog.hxx"


RejectTaskHandler::RejectTaskHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : TaskHandlerBase(Operation::POST_Task_id_reject, allowedProfessionOiDs)
{
}

void RejectTaskHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto prescriptionId = parseId(session.request, session.accessLog);

    TVLOG(1) << "Working on Task for prescription id " << prescriptionId.toString();

    auto* databaseHandle = session.database();
    auto taskAndKey = databaseHandle->retrieveTaskForUpdate(prescriptionId);

    ErpExpect(taskAndKey.has_value(), HttpStatus::NotFound, "Task not found for prescription id");

    auto& task = taskAndKey->task;
    const auto taskStatus = task.status();
    ErpExpect(taskStatus != model::Task::Status::cancelled, HttpStatus::Gone, "Task has already been deleted");

    A_19171_03.start("Check that Task is in progress");
    ErpExpect(taskStatus == model::Task::Status::inprogress, HttpStatus::Forbidden,
              "Task not in status in progress, is: " + std::string(model::Task::StatusNames.at(taskStatus)));
    A_19171_03.finish();

    A_19171_03.start("Check secret");
    const auto uriSecret = session.request.getQueryParameter("secret");
    A_20703.start("Set VAU-Error-Code header field to brute_force whenever AccessCode or Secret mismatches");
    VauExpect(uriSecret.has_value() && uriSecret.value() == task.secret(), HttpStatus::Forbidden,
              VauErrorCode::brute_force, "No or invalid secret");
    A_20703.finish();
    A_19171_03.finish();

    A_19172_01.start("Delete secret from Task");
    task.deleteSecret();
    A_19172_01.finish();

    // GEMREQ-start A_24175#delete-task
    A_24175.start("Delete owner from Task");
    task.deleteOwner();
    A_24175.finish();
    // GEMREQ-end A_24175#delete-task

    A_19172_01.start("Set Task status to ready");
    task.setStatus(model::Task::Status::ready);
    A_19172_01.finish();

    task.updateLastUpdate();
    // Update task in database
    ErpExpect(taskAndKey->key.has_value(), HttpStatus::InternalServerError, "Missing key for task");
    databaseHandle->updateTaskStatusAndSecret(task, *taskAndKey->key);

    // GEMREQ-start A_24286#delete-MedicationDispense
    A_24286_02.start("Delete MedicationDispense");
    if(task.lastMedicationDispense().has_value())
    {
        task.deleteLastMedicationDispense();
        databaseHandle->updateTaskDeleteMedicationDispense(task);
    }
    A_24286_02.finish();
    // GEMREQ-end A_24286#delete-MedicationDispense

    A_19514.start("HttpStatus 204 for successful POST");
    makeResponse(session, HttpStatus::NoContent, nullptr/*body*/);
    A_19514.finish();

    // Collect Audit data
    const auto kvnr = task.kvnr();
    Expect3(kvnr.has_value(), "Task has no KV number", std::logic_error);
    session.auditDataCollector()
        .setEventId(model::AuditEventId::POST_Task_reject)
        .setInsurantKvnr(*kvnr)
        .setAction(model::AuditEvent::Action::update)
        .setPrescriptionId(prescriptionId);
}
