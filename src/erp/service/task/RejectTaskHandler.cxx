/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/task/RejectTaskHandler.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/util/TLog.hxx"


RejectTaskHandler::RejectTaskHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : TaskHandlerBase(Operation::POST_Task_id_reject, allowedProfessionOiDs)
{
}

void RejectTaskHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto prescriptionId = parseId(session.request, session.accessLog);
    checkFeatureWf200(prescriptionId.type());

    TVLOG(1) << "Working on Task for prescription id " << prescriptionId.toString();

    auto* databaseHandle = session.database();
    auto task = databaseHandle->retrieveTaskForUpdate(prescriptionId);

    ErpExpect(task.has_value(), HttpStatus::NotFound, "Task not found for prescription id");

    const auto taskStatus = task->status();
    ErpExpect(taskStatus != model::Task::Status::cancelled, HttpStatus::Gone, "Task has already been deleted");

    A_19171.start("Check that Task is in progress");
    ErpExpect(taskStatus == model::Task::Status::inprogress, HttpStatus::Forbidden,
              "Task not in status in progress, is: " + std::string(model::Task::StatusNames.at(taskStatus)));
    A_19171.finish();

    A_19171.start("Check secret");
    const auto uriSecret = session.request.getQueryParameter("secret");
    A_20703.start("Set VAU-Error-Code header field to brute_force whenever AccessCode or Secret mismatches");
    VauExpect(uriSecret.has_value() && uriSecret.value() == task->secret(), HttpStatus::Forbidden,
              VauErrorCode::brute_force, "No or invalid secret");
    A_20703.finish();
    A_19171.finish();

    A_19172.start("Delete secret from Task");
    task->deleteSecret();
    A_19172.finish();

    A_19172.start("Set Task status to ready");
    task->setStatus(model::Task::Status::ready);
    A_19172.finish();

    task->updateLastUpdate();
    // Update task in database
    databaseHandle->updateTaskStatusAndSecret(*task);

    A_19514.start("HttpStatus 204 for successful POST");
    makeResponse(session, HttpStatus::NoContent, nullptr/*body*/);
    A_19514.finish();

    // Collect Audit data
    const auto kvnr = task->kvnr();
    Expect3(kvnr.has_value(), "Task has no KV number", std::logic_error);
    session.auditDataCollector()
        .setEventId(model::AuditEventId::POST_Task_reject)
        .setInsurantKvnr(*kvnr)
        .setAction(model::AuditEvent::Action::update)
        .setPrescriptionId(prescriptionId);
}
