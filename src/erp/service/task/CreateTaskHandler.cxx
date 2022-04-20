/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/task/CreateTaskHandler.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/crypto/SecureRandomGenerator.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/Parameters.hxx"
#include "erp/model/Task.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/TLog.hxx"

CreateTaskHandler::CreateTaskHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs)
        : TaskHandlerBase(Operation::POST_Task_create, allowedProfessionOiDs)
{
}

void CreateTaskHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();
    TVLOG(2) << "request body is '" << session.request.getBody() << "'";

    A_19021.start("generate 256Bit random access code");
    const auto accessCode = ByteHelper::toHex(SecureRandomGenerator::generate(32));
    A_19021.finish();

    A_19257.start("validate against schema");
    const auto parameters = parseAndValidateRequestBody<model::Parameters>(session, SchemaType::CreateTaskParameters);
    A_19257.finish();

    A_19112.start("extract and check workFlowType parameter for the prescription type of the task");
    auto prescriptionType = parameters.getPrescriptionType();
    ErpExpect(prescriptionType.has_value(), HttpStatus::BadRequest, "Invalid workFlowType in incoming parameters");
    A_19112.finish();
    checkFeatureWf200(*prescriptionType);
    model::Task task(*prescriptionType, accessCode);

    auto databaseHandle = session.database();
    const auto prescriptionId = databaseHandle->storeTask(task);

    // Note that the prescription ID is not yet permanently stored inside the encrypted task-bundle,
    // because we want to avoid a second database access for only storing this redundant information.
    // The prescriptionId will be persisted during $activate inside the task bundle.
    task.setPrescriptionId(prescriptionId);

    session.accessLog.prescriptionId(prescriptionId.toString());

    A_19114.start("return the created Task");
    makeResponse(session, HttpStatus::Created, &task);
    A_19114.finish();
}
