/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/task/DispenseTaskHandler.hxx"

#include "shared/ErpRequirements.hxx"
#include "erp/model/MedicationsAndDispenses.hxx"
#include "erp/service/task/CloseTaskHandler.hxx"
#include "erp/service/MedicationDispenseHandler.hxx"
#include "erp/service/MedicationDispenseHandlerBase.hxx"
#include "shared/model/MedicationDispense.hxx"
#include "shared/model/MedicationDispenseBundle.hxx"

/**
 * @brief Construct a new Dispense Task Handler:: Dispense Task Handler object
 *
 * @param allowedProfessionOiDs
 */
DispenseTaskHandler::DispenseTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : MedicationDispenseHandlerBase(Operation::POST_Task_id_dispense, allowedProfessionOiDs)
{
}

/**
 * @brief handle request from client
 *
 * @param session
 */
void DispenseTaskHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();
    TVLOG(2) << "request body is '" << session.request.getBody() << "'";

    const auto prescriptionId = parseId(session.request, session.accessLog);

    TVLOG(1) << "Working on Task for prescription id " << prescriptionId.toString();

    auto* databaseHandle = session.database();

    auto [taskAndKey, prescription] = databaseHandle->retrieveTaskForUpdateAndPrescription(prescriptionId);
    ErpExpect(taskAndKey.has_value(), HttpStatus::NotFound, "Task not found for prescription id");
    auto& task = taskAndKey->task;

    // GEMREQ-start A_24280
    A_24280.start("Check that secret from URL is equal to secret from task");
    const auto uriSecret = session.request.getQueryParameter("secret");
    A_20703.start("Set VAU-Error-Code header field to brute_force whenever AccessCode or Secret mismatches");
    VauExpect(uriSecret.has_value() && uriSecret.value() == task.secret(), HttpStatus::Forbidden,
              VauErrorCode::brute_force, "No or invalid secret provided for Task");
    A_20703.finish();
    A_24280.finish();
    // GEMREQ-end A_24280

    // GEMREQ-start A_24298
    A_24298.start("Check that task is in progress");
    const auto taskStatus = task.status();
    ErpExpect(taskStatus == model::Task::Status::inprogress, HttpStatus::Forbidden, "Task not in-progress.");
    A_24298.finish();
    // GEMREQ-end A_24298

    const auto& accessToken = session.request.getAccessToken();
    const auto telematikIdFromAccessToken = accessToken.stringForClaim(JWT::idNumberClaim);
    ErpExpect(telematikIdFromAccessToken.has_value(), HttpStatus::BadRequest, "Telematik-ID not contained in JWT");
    const auto kvnr = task.kvnr();
    Expect3(kvnr.has_value(), "Task has no KV number", std::logic_error);

    auto bodyData = parseBody(session, Operation::POST_Task_id_dispense, prescriptionId.type());
    A_24281_02.start("Check provided MedicationDispense object, especially PrescriptionID, KVNR and TelematikID");
    checkMedicationDispenses(bodyData.medicationDispenses, prescriptionId, kvnr.value(), telematikIdFromAccessToken.value());
    A_24281_02.finish();

    A_24285_01.start("Zeitpunkt des Aufrufes in Task.extension:lastMedicationDispense setzen");
    task.updateLastMedicationDispense();
    A_24285_01.finish();

    A_24281_02.start("Save modified Task and MedicationDispense");
    task.updateLastUpdate();
    ErpExpect(taskAndKey->key.has_value(), HttpStatus::InternalServerError, "Missing key for task");
    auto bundle = MedicationDispenseHandlerBase::createBundle(bodyData);
    databaseHandle->updateTaskMedicationDispense(task, bundle);
    A_24281_02.finish();

    // Response
    makeResponse(session, HttpStatus::OK, &bundle);

    // Collect Audit data
    session.auditDataCollector()
        .setEventId(model::AuditEventId::POST_Task_dispense)
        .setInsurantKvnr(*kvnr)
        .setAction(model::AuditEvent::Action::update)
        .setPrescriptionId(prescriptionId);
}
