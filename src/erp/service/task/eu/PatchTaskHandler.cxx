/*
* (C) Copyright IBM Deutschland GmbH 2021, 2025
* (C) Copyright IBM Corp. 2021, 2025
*
* non-exclusively licensed to gematik GmbH
*/

#include "erp/model/eu/GemErpEuPrTaskInput.hxx"
#include "erp/service/task/eu/PatchTaskHandler.hxx"
#include "erp/util/search/UrlArguments.hxx"

#include <optional>

namespace eu {



PatchTaskHandler::PatchTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : TaskHandlerBase(Operation::PATCH_Task_id_mark, allowedProfessionOiDs)
{

}

void PatchTaskHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    A_27548.start("Check for id parameter (parseId), disallow operation without id (throws).");
    const auto prescriptionId = parseId(session.request, session.accessLog);
    A_27548.finish();

    TVLOG(1) << "Working on Task for prescription id " << prescriptionId.toString();

    // Check for kvnr presence:
    const auto& accessToken = session.request.getAccessToken();
    const auto kvnrClaim = accessToken.stringForClaim(JWT::idNumberClaim);
    ErpExpect(kvnrClaim.has_value(), HttpStatus::BadRequest, "Missing claim in ACCESS_TOKEN: " + std::string(JWT::idNumberClaim));

    A_27551.start("FHIR validation with GEM_ERPEU_PR_PAR_PATCH_Task_Input profile.");
    const auto validatedDoc = parseAndValidateRequestBody<model::GemErpEuPrTaskInput>(session);
    A_27551.finish();

    A_27550.start("Versicherte können nur ihre eigenen Tasks markieren.");
    auto* databaseHandle = session.database();
    const model::Kvnr kvnr{*kvnrClaim};
    auto taskAndKey = databaseHandle->retrieveTaskForUpdate(prescriptionId);
    ErpExpect(taskAndKey.has_value(), HttpStatus::NotFound, "No such task.");
    model::Task& task{taskAndKey->task};
    ErpExpect(task.kvnr() && task.kvnr() == kvnr && task.status() == model::Task::Status::ready, HttpStatus::Forbidden,
              "Operation only allowed by insurant owning the task and task status is ready");
    A_27550.finish();

    A_28500.start("Task markieren - Versicherter - nur einlösbare E-Rezepte");
    try
    {
        ErpExpect(task.isEuRedeemableByProperties(), HttpStatus::Conflict, "Only eu redeemable prescriptions allowed.");
    }
    catch (const model::ModelException& exception)
    {
        ErpFail(HttpStatus::Conflict, "Only eu redeemable prescriptions allowed.");
    }
    A_28500.finish();

    A_27552.start("Update task redeemable flag.");
    task.updateLastUpdate();
    databaseHandle->setTaskEuRedeemableByPatient(task, validatedDoc);
    task.setEuRedeemableByPatient(validatedDoc.isEuRedeemableByPatientAuthorization());
    A_27552.finish();

    makeResponse(session, HttpStatus::OK, &task);

    // Collect Audit data
    session.auditDataCollector()
        .setEventId(model::AuditEventId::PATCH_TASK_ID_MARK)
        .setInsurantKvnr(kvnr)
        .setAction(model::AuditEvent::Action::update)
        .setPrescriptionId(prescriptionId);
}

} // eu
