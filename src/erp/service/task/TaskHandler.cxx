/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "TaskHandler.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/crypto/Jws.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/Device.hxx"
#include "erp/model/ModelException.hxx"
#include "erp/model/Signature.hxx"
#include "erp/model/Task.hxx"
#include "erp/service/AuditEventCreator.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/search/UrlArguments.hxx"


TaskHandlerBase::TaskHandlerBase(const Operation operation,
                                 const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : ErpRequestHandler(operation, allowedProfessionOIDs)
{
}

void TaskHandlerBase::addToPatientBundle(model::Bundle& bundle, const model::Task& task,
                                         const std::optional<model::Bundle>& patientConfirmation,
                                         const std::optional<model::Bundle>& receipt)
{
    bundle.addResource(makeFullUrl("/Task/" + task.prescriptionId().toString()),
                       {}, {}, task.jsonDocument());

    if (patientConfirmation.has_value())
    {
        bundle.addResource({}, {}, {}, patientConfirmation->jsonDocument());
    }
    if (receipt.has_value())
    {
        bundle.addResource({}, {}, {}, receipt->jsonDocument());
    }
}


model::Bundle TaskHandlerBase::convertToPatientConfirmation(const model::Binary& healthcareProviderPrescription,
                                                            const Uuid& uuid, PcServiceContext& serviceContext)
{
    A_19029_02.start("1. convert prescription bundle to JSON");
    Expect3(healthcareProviderPrescription.data().has_value(), "healthcareProviderPrescription unexpected empty Binary",
            std::logic_error);

    // read CAdES-BES signature, but do not verify it
    const CadesBesSignature cadesBesSignature =
        CadesBesSignature(std::string(*healthcareProviderPrescription.data()), nullptr);

    // this comes from the database and has already been validated when initially received
    auto bundle = model::Bundle::fromXmlNoValidation(cadesBesSignature.payload());
    A_19029_02.finish();

    A_19029_02.start("assign identifier");
    bundle.setId(uuid);
    A_19029_02.finish();

    A_19029_02.start("2. serialize to normalized JSON");
    auto normalizedJson = bundle.serializeToCanonicalJsonString();
    A_19029_02.finish();

    A_19029_02.start("3. sign the JSON bundle using C.FD.SIG");
    JoseHeader joseHeader(JoseHeader::Algorithm::BP256R1);
    joseHeader.setX509Certificate(serviceContext.getCFdSigErp());
    joseHeader.setType(ContentMimeType::jose);
    joseHeader.setContentType(ContentMimeType::fhirJsonUtf8);

    const Jws jsonWebSignature(joseHeader, normalizedJson, serviceContext.getCFdSigErpPrv());
    const auto signatureData = jsonWebSignature.compactDetachedSerialized();
    A_19029_02.finish();

    A_19029_02.start("store the signature in the bundle");
    model::Signature signature(signatureData, model::Timestamp::now(), model::Device::createReferenceString(getLinkBase()));
    signature.setTargetFormat(MimeType::fhirJson);
    signature.setSigFormat(MimeType::jose);
    signature.setType(model::Signature::jwsSystem, model::Signature::jwsCode);
    bundle.setSignature(signature);
    A_19029_02.finish();

    return bundle;
}


model::PrescriptionId TaskHandlerBase::parseId(const ServerRequest& request, AccessLog& accessLog)
{
    const auto prescriptionIdValue = request.getPathParameter("id");
    ErpExpect(prescriptionIdValue.has_value(), HttpStatus::BadRequest, "id path parameter is missing");

    try
    {
        auto prescriptionId = model::PrescriptionId::fromString(prescriptionIdValue.value());
        accessLog.prescriptionId(prescriptionIdValue.value());
        return prescriptionId;
    }
    catch (const model::ModelException&)
    {
        ErpFail(HttpStatus::NotFound, "Failed to parse prescription ID from URL parameter");
    }
}

void TaskHandlerBase::checkAccessCodeMatches(const ServerRequest& request, const model::Task& task)
{
    auto accessCode = request.getQueryParameter("ac");
    if (! accessCode.has_value())
    {
        accessCode = request.header().header(Header::XAccessCode);
    }
    A_20703.start("Set VAU-Error-Code header field to brute-force whenever AccessCode or Secret mismatches");
    VauExpect(accessCode == task.accessCode(), HttpStatus::Forbidden, VauErrorCode::brute_force,
              "AccessCode mismatch");
    A_20703.finish();
}

GetAllTasksHandler::GetAllTasksHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : TaskHandlerBase(Operation::GET_Task, allowedProfessionOIDs)
{
}


void GetAllTasksHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto& accessToken = session.request.getAccessToken();

    A_19115.start("retrieve KVNR from ACCESS_TOKEN");
    const auto kvnr = accessToken.stringForClaim(JWT::idNumberClaim);
    ErpExpect(kvnr.has_value(), HttpStatus::BadRequest,
              "Missing claim in ACCESS_TOKEN: " + std::string(JWT::idNumberClaim));
    A_19115.finish();

    auto arguments = std::optional<UrlArguments>(
        std::in_place,
        std::vector<SearchParameter>
            {
                {"status", "status",SearchParameter::Type::TaskStatus},
                {"authored-on", "authored_on", SearchParameter::Type::Date},
                {"modified", "last_modified",SearchParameter::Type::Date},
            });
    arguments->parse(session.request, session.serviceContext.getKeyDerivation());

    auto* databaseHandle = session.database();
    A_19115.start("use KVNR to filter tasks");
    auto resultSet = databaseHandle->retrieveAllTasksForPatient(kvnr.value(), arguments);
    A_19115.finish();

    model::Bundle responseBundle(model::Bundle::Type::searchset);
    const auto links = arguments->getBundleLinks(getLinkBase(), "/Task");
    for (const auto& link : links)
    {
        responseBundle.setLink(link.first, link.second);
    }
    if (! resultSet.empty())
    {
        A_19129_01.start("create response bundle for patient");

        for (const auto& task : resultSet)
        {
            Expect(task.status() != model::Task::Status::cancelled, "should already be filtered by SQL query.");
            // in C_10499 the response bundle has changed to only contain tasks, no patient confirmation
            // and no receipt any longer
            responseBundle.addResource(makeFullUrl("/Task/" + task.prescriptionId().toString()),
                {}, model::Bundle::SearchMode::match, task.jsonDocument());
        }
    }

    std::size_t totalSearchMatches =
        responseIsPartOfMultiplePages(arguments->pagingArgument(), resultSet.size()) ?
        databaseHandle->countAllTasksForPatient(kvnr.value(), arguments) : resultSet.size();
    responseBundle.setTotalSearchMatches(totalSearchMatches);

    A_19514.start("HttpStatus 200 for successful GET");
    makeResponse(session, HttpStatus::OK, &responseBundle);
    A_19514.finish();

    // Collect Audit data
    session.auditDataCollector()
        .setEventId(model::AuditEventId::GET_Task)
        .setInsurantKvnr(*kvnr)
        .setAction(model::AuditEvent::Action::read);
}


GetTaskHandler::GetTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : TaskHandlerBase(Operation::GET_Task_id, allowedProfessionOIDs)
{
}

void GetTaskHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto prescriptionId = parseId(session.request, session.accessLog);

    const auto& accessToken = session.request.getAccessToken();

    if (accessToken.stringForClaim(JWT::professionOIDClaim) == profession_oid::oid_versicherter)
    {
        handleRequestFromPatient(session, prescriptionId, accessToken);
    }
    else
    {
        handleRequestFromPharmacist(session, prescriptionId);
    }

    // Collect Audit data
    session.auditDataCollector().setAction(model::AuditEvent::Action::read).setPrescriptionId(prescriptionId);
}

void GetTaskHandler::handleRequestFromPatient(PcSessionContext& session, const model::PrescriptionId& prescriptionId,
                                              const JWT& accessToken)
{
    auto* databaseHandle = session.database();
    auto [task, healthcareProviderPrescription] =
        databaseHandle->retrieveTaskAndPrescription(prescriptionId);

    checkTaskState(task);

    const auto kvnr = task->kvnr();
    Expect3(kvnr.has_value(), "Task has no KV number", std::logic_error);

    model::AuditEventId auditEventId = model::AuditEventId::GET_Task_id_insurant;

    A_19116.start("check if the KVNR matches");
    const auto kvnrFromAccessToken = accessToken.stringForClaim(JWT::idNumberClaim);
    if (kvnrFromAccessToken != kvnr)
    {
        auditEventId = model::AuditEventId::GET_Task_id_representative;

        A_19116.finish();
        A_19116.start("on KVNR missmatch check if the access code matches");
        checkAccessCodeMatches(session.request, *task);
        A_19116.finish();

        A_20753.start("Exclusion of representative access to or using verification identity");
        if (kvnrFromAccessToken.has_value())
        {
            ErpExpect(isVerificationIdentityKvnr(kvnrFromAccessToken.value()) == isVerificationIdentityKvnr(kvnr.value()),
                HttpStatus::BadRequest,
                "KVNR verification identities may not access information from insurants and vice versa");
        }
        A_20753.finish();
    }

    std::optional<model::Bundle> patientConfirmation;
    if (healthcareProviderPrescription.has_value())
    {
        auto confirmationId = task->patientConfirmationUuid();
        Expect3(confirmationId.has_value(), "patient confirmation ID not set in task", std::logic_error);
        patientConfirmation = convertToPatientConfirmation(*healthcareProviderPrescription, Uuid(*confirmationId),
                                                           session.serviceContext);
    }

    A_20702_01.start("remove access code from task if client is unknown");
    // Unknown clients are filtered out by the VAU Proxy, nothing to do in the erp-processing-context.
    A_20702_01.finish();

    A_21375.start("create response bundle for patient");
    model::Bundle responseBundle(model::Bundle::Type::collection);
    responseBundle.setLink(model::Link::Type::Self, makeFullUrl("/Task/") + task.value().prescriptionId().toString());
    addToPatientBundle(responseBundle, task.value(), patientConfirmation, {});
    A_21375.finish();

    A_19569_02.start("_revinclude: reverse include referencing audit events");
    UrlArguments urlArguments({});
    urlArguments.parse(session.request, session.serviceContext.getKeyDerivation());
    if (urlArguments.hasReverseIncludeAuditEventArgument())
    {
        const auto auditDatas = databaseHandle->retrieveAuditEventData(std::string(*kvnr), {}, prescriptionId, {});
        for (const auto& auditData : auditDatas)
        {
            const auto language = getLanguageFromHeader(session.request.header());
            const auto auditEvent = AuditEventCreator::fromAuditData(
                auditData, language, session.serviceContext.auditEventTextTemplates(),
                session.request.getAccessToken());

            responseBundle.addResource({}, {}, {}, auditEvent.jsonDocument());
        }
    }
    A_19569_02.finish();

    makeResponse(session, HttpStatus::OK, &responseBundle);

    // Collect Audit data
    session.auditDataCollector()
        .setEventId(auditEventId)
        .setInsurantKvnr(*kvnr);
}

void GetTaskHandler::handleRequestFromPharmacist(PcSessionContext& session, const model::PrescriptionId& prescriptionId)
{
    auto* databaseHandle = session.database();
    const auto [task, receipt] = databaseHandle->retrieveTaskAndReceipt(prescriptionId);

    checkTaskState(task);

    A_19226.start("create response bundle for pharmacist");
    A_20703.start("Set VAU-Error-Code header field to brute-force whenever AccessCode or Secret mismatches");
    const auto uriSecret = session.request.getQueryParameter("secret");
    VauExpect(uriSecret.has_value() && uriSecret.value() == task->secret(),
        HttpStatus::Forbidden, VauErrorCode::brute_force,
        "No or invalid secret provided for user pharmacy");
    A_20703.finish();

    const auto selfLink = makeFullUrl("/Task/" + task.value().prescriptionId().toString());
    model::Bundle responseBundle(model::Bundle::Type::collection);
    responseBundle.setLink(model::Link::Type::Self, selfLink);
    responseBundle.addResource(
        selfLink, {}, {}, task->jsonDocument());

    if (task->status() == model::Task::Status::completed && receipt.has_value())
    {
        responseBundle.addResource({}, {}, {}, receipt->jsonDocument());
    }

    A_19514.start("HttpStatus 200 for successful GET");
    makeResponse(session, HttpStatus::OK, &responseBundle);
    A_19514.finish();

    // Collect Audit data
    const auto kvnr = task->kvnr();
    Expect3(kvnr.has_value(), "Task has no KV number", std::logic_error);
    session.auditDataCollector()
        .setEventId(model::AuditEventId::GET_Task_id_pharmacy)
        .setInsurantKvnr(*kvnr);
}

void GetTaskHandler::checkTaskState(const std::optional<model::Task>& task)
{
    ErpExpect(task.has_value(), HttpStatus::NotFound, "Task not found in DB");
    A_18952.start("don't return deleted mTasks");
    ErpExpect(task->status() != model::Task::Status::cancelled, HttpStatus::Gone, "Task has already been deleted");
    A_18952.finish();
}
