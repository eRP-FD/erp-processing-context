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

    A_19257_01.start("validate against schema");
    const auto parameters = parseAndValidateRequestBody<model::Parameters>(session, SchemaType::CreateTaskParameters);
    A_19257_01.finish();

    A_19112.start("extract and check workFlowType parameter for the prescription type of the task");
    ErpExpect(parameters.count() == 1, HttpStatus::BadRequest, "unexpected number of parameters");
    ErpExpect(parameters.hasParameter("workflowType"), HttpStatus::BadRequest, "workflowType Parameter not given");
    bool supportedWorkflow = false;
    ErpExpect(parameters.getWorkflowSystem().has_value(), HttpStatus::BadRequest, "missing workflow system");
    for (const auto bundle : model::ResourceVersion::supportedBundles())
    {
        if ((bundle == model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01 &&
             parameters.getWorkflowSystem().value() == model::resource::code_system::deprecated::flowType) ||
            parameters.getWorkflowSystem().value() == model::resource::code_system::flowType)
        {
            supportedWorkflow = true;
        }
    }
    ErpExpect(supportedWorkflow == true, HttpStatus::BadRequest, "unknown flowType system given" + std::string{parameters.getWorkflowSystem().value()});

    auto prescriptionType = [&parameters] {
        try
        {
            return parameters.getPrescriptionType();
        }
        catch (const model::ModelException& me)
        {
            ErpFailWithDiagnostics(HttpStatus::BadRequest, "error getting workFlowType from parameters", me.what());
        }
    }();
    ErpExpect(prescriptionType.has_value(), HttpStatus::BadRequest, "Invalid workFlowType in incoming parameters");
    A_19112.finish();

    // GEMREQ-start A_19021-02
    A_19021_02.start("generate 256Bit random access code and add to task");
    const auto accessCode = ByteHelper::toHex(SecureRandomGenerator::generate(32));
    model::Task task(*prescriptionType, accessCode);
    A_19021_02.finish();

    auto* databaseHandle = session.database();
    const auto prescriptionId = databaseHandle->storeTask(task);
    // GEMREQ-end A_19021-02

    // Note that the prescription ID is not yet permanently stored inside the encrypted task-bundle,
    // because we want to avoid a second database access for only storing this redundant information.
    // The prescriptionId will be persisted during $activate inside the task bundle.
    task.setPrescriptionId(prescriptionId);

    session.accessLog.prescriptionId(prescriptionId.toString());

    A_19114.start("return the created Task");
    makeResponse(session, HttpStatus::Created, &task);
    A_19114.finish();
}
