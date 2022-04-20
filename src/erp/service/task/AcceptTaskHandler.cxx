/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/task/AcceptTaskHandler.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/crypto/SecureRandomGenerator.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/ByteHelper.hxx"

#include "erp/common/MimeType.hxx"


AcceptTaskHandler::AcceptTaskHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : TaskHandlerBase(Operation::POST_Task_id_accept, allowedProfessionOiDs)
{
}

void AcceptTaskHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto prescriptionId = parseId(session.request, session.accessLog);
    checkFeatureWf200(prescriptionId.type());

    TVLOG(1) << "Working on Task for prescription id " << prescriptionId.toString();

    auto databaseHandle = session.database();

    auto [task, healthCareProviderPrescription] = databaseHandle->retrieveTaskAndPrescription(prescriptionId);
    ErpExpect(task.has_value(), HttpStatus::NotFound, "Task not found for prescription id");

    checkTaskPreconditions(session, *task);

    ErpExpect(healthCareProviderPrescription.has_value(), HttpStatus::NotFound,
        "Healthcare provider prescription not found for prescription id");

    A_19169.start("Set status to in-progress, create and set secret");
    task->setStatus(model::Task::Status::inprogress);
    const auto secret = SecureRandomGenerator::generate(32);
    task->setSecret(ByteHelper::toHex(secret));
    task->updateLastUpdate();
    A_19169.finish();

    databaseHandle->updateTaskStatusAndSecret(*task);

    // Create response:
    const auto linkBase = makeFullUrl("/Task/" + prescriptionId.toString());
    model::Bundle responseBundle(model::BundleType::collection, ::model::ResourceBase::NoProfile);
    responseBundle.setLink(model::Link::Type::Self, linkBase + "/$accept/");
    responseBundle.addResource(linkBase, {}, {}, task->jsonDocument());
    std::string uuid{};
    if (healthCareProviderPrescription->id().has_value())
    {
        uuid = Uuid{healthCareProviderPrescription->id().value()}.toUrn();
    }
    responseBundle.addResource(uuid, {}, {}, healthCareProviderPrescription->jsonDocument());

    const auto kvnr = task->kvnr();
    Expect3(kvnr.has_value(), "Task has no KV number", std::logic_error);

    if(Configuration::instance().featurePkvEnabled() &&
       prescriptionId.type() == model::PrescriptionType::apothekenpflichtigeArzneimittelPkv)
    {
        A_22110.start("Evaluate consent for flowtype 200 (PKV prescription) and add to response bundle");
        const auto consent = databaseHandle->retrieveConsent(kvnr.value());
        if (consent.has_value())
        {
            TVLOG(1) << "Consent found for Kvnr of task";
            Expect3(consent->id().has_value(), "Consent id not set", std::logic_error);
            responseBundle.addResource(makeFullUrl("/Consent/" + std::string(consent->id().value())), {}, {},
                                       consent->jsonDocument());
        }
        A_22110.finish();
    }

    A_19514.start("HttpStatus 200 for successful POST");
    makeResponse(session, HttpStatus::OK, &responseBundle);
    A_19514.finish();

    // Collect Audit data
    session.auditDataCollector()
        .setEventId(model::AuditEventId::POST_Task_accept)
        .setInsurantKvnr(kvnr.value())
        .setAction(model::AuditEvent::Action::update)
        .setPrescriptionId(prescriptionId);
}


void AcceptTaskHandler::checkTaskPreconditions(const PcSessionContext& session, const model::Task& task)
{
    std::string taskAccessCode;
    try
    {
        taskAccessCode = task.accessCode();
    }
    catch (const model::ModelException&)
    {
        // handled by taskAccessCode.empty() below.
    }

    const auto taskStatus = task.status();

    A_19149.start("Check if task meanwhile deleted");
    if(taskAccessCode.empty() ||
       taskStatus == model::Task::Status::cancelled)
    {
        ErpFail(HttpStatus::Gone, "Task meanwhile deleted for prescription id");
    }
    A_19149.finish();

    A_19167_01.start("Check if access code from URL is equal to access code from task");
    checkAccessCodeMatches(session.request, task);
    A_19167_01.finish();

    A_19168.start("Check if task has correct status");
    if(taskStatus == model::Task::Status::completed ||
       taskStatus == model::Task::Status::inprogress ||
       taskStatus == model::Task::Status::draft)
    {
        ErpFail(HttpStatus::Conflict,
                "Task has invalid status " + std::string(model::Task::StatusNames.at(taskStatus)));
    }
    A_19168.finish();
}
