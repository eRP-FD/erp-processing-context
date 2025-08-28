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
    std::optional<model::Task> task;
    if (taskAndKey.has_value())
    {
        task = std::move(taskAndKey->task);
    }
    Expect3(task->kvnr().has_value(), "Task has no KV number", std::logic_error);
    ErpExpect(task->kvnr() == kvnr, HttpStatus::Forbidden, "Only insurant prescription allowed.");
    A_27550.finish();

    try
    {
        ErpExpect(task->isEuRedeemableByProperties(), HttpStatus::Forbidden, "Only eu redeemable prescriptions allowed.");
    }
    catch (const model::ModelException& exception)
    {
        ErpFail(HttpStatus::Forbidden, "Only eu redeemable prescriptions allowed.");
    }

    A_27552.start("Update task redeemable flag.");
    task->updateLastUpdate();
    databaseHandle->setTaskEuRedeemableByPatient(*task, validatedDoc);
    task->setEuRedeemableByPatient(validatedDoc.isEuRedeemableByPatientAuthorization());
    A_27552.finish();

    makeResponse(session, HttpStatus::OK, std::addressof(task.value()));

    // Collect Audit data
    session.auditDataCollector()
        .setEventId(model::AuditEventId::PATCH_TASK_ID_MARK)
        .setInsurantKvnr(kvnr)
        .setAction(model::AuditEvent::Action::update)
        .setPrescriptionId(prescriptionId);
}

} // eu
