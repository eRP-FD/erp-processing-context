/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "erp/service/task/GetTaskHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/compression/Deflate.hxx"
#include "erp/hsm/VsdmKeyCache.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/service/AuditEventCreator.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Hash.hxx"
#include "erp/util/String.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "erp/xml/XmlDocument.hxx"


namespace
{
using namespace std::chrono_literals;
// the xsd can be found at https://github.com/gematik/api-telematik/blob/OPB4/fa/vsds/Pruefungsnachweis.xsd
constexpr std::string_view xPathExpression_TSText = "/ns:PN/ns:TS/text()";
constexpr std::string_view xPathExpression_EText = "/ns:PN/ns:E/text()";
constexpr std::string_view xPathExpression_PZText = "/ns:PN/ns:PZ/text()";

// GEMREQ-start proofContent
struct ProofContent
{
    model::Kvnr kvnr{""};
    std::optional<model::Timestamp> timestamp;
    char keyOperatorId{};
    char keyVersion{};
    std::string truncatedHmac{};
    std::string validationDataForHash;
};

struct ProofInformation {
    model::Timestamp timestamp;
    std::string result{};
    // Base64 encoded data
    std::optional<std::string> proofContentBase64;
    std::optional<ProofContent> proofContent;

    static constexpr std::size_t posVsdmKeyOperatorId = 21;
    static constexpr std::size_t posVsdmKeyVersion = 22;
    // length of the data we have to calculate the HMAC of
    static constexpr std::size_t dataSize = 23;
    // length of the truncated HMAC
    static constexpr std::size_t truncatedHmacSize = 24;
    static constexpr std::size_t kvnrSize = 10;
    static constexpr std::size_t timestampSize = 10;
};
// GEMREQ-end proofContent

ProofContent extractProofContent(std::string_view proofContentBase64)
{
    const auto validationDataBinary = Base64::decodeToString(proofContentBase64);
    ErpExpect(validationDataBinary.size() == ProofInformation::dataSize + ProofInformation::truncatedHmacSize,
              HttpStatus::Forbidden, "Invalid size of Prüfziffer");
    std::string_view validationData{validationDataBinary};

    ProofContent proofContent;
    proofContent.kvnr = model::Kvnr{validationData.substr(0, ProofInformation::kvnrSize)};
    proofContent.keyOperatorId = validationData[ProofInformation::posVsdmKeyOperatorId];
    proofContent.keyVersion = validationData[ProofInformation::posVsdmKeyVersion];

    std::string_view timestampStr = validationData.substr(ProofInformation::kvnrSize, ProofInformation::timestampSize);
    std::uint32_t unixTimestamp{};
    const auto* endPtr = timestampStr.data() + timestampStr.size();
    auto res = std::from_chars(timestampStr.data(), endPtr, unixTimestamp);
    if (res.ec == std::errc() && res.ptr == endPtr)
    {
        proofContent.timestamp = model::Timestamp(static_cast<int64_t>(unixTimestamp));
    }
    proofContent.truncatedHmac = validationData.substr(ProofInformation::dataSize, ProofInformation::truncatedHmacSize);
    proofContent.validationDataForHash = validationData.substr(0, ProofInformation::dataSize);

    return proofContent;
}

/**
 * Extract the proof information from the given xml data. The data is
 * only minimally validated, i.e. if the given data conforms to the
 * xsd and if the expected data size of the PZ value is correct.
 *
 * Proper validation of the returned content must happen afterwards.
 */
ProofInformation parseProofInformation(const SessionContext& context, const std::string& pnw)
{
    try
    {
        XmlDocument pnwDocument{pnw};
        pnwDocument.registerNamespace("ns", "http://ws.gematik.de/fa/vsdm/pnw/v1.0");
        const auto& xmlValidator = context.serviceContext.getXmlValidator();
        std::unique_ptr<XmlValidatorContext> validatorContext =
            xmlValidator.getSchemaValidationContext(SchemaType::Pruefungsnachweis);
        pnwDocument.validateAgainstSchema(*validatorContext);

        const auto timestamp = pnwDocument.getElementText(xPathExpression_TSText.data());
        auto pzText = pnwDocument.getOptionalElementText(xPathExpression_PZText.data());
        std::optional<ProofContent> proofContent;
        if (pzText.has_value())
        {
            proofContent = extractProofContent(*pzText);
        }
        return ProofInformation{.timestamp = model::Timestamp::fromDtmDateTime(timestamp),
                                .result = pnwDocument.getElementText(xPathExpression_EText.data()),
                                .proofContentBase64 = std::move(pzText),
                                .proofContent = std::move(proofContent)};
    }
    catch (const std::exception& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::Forbidden, "Failed parsing PNW XML.", ex.what());
    }
}

/**
 * This function performs the decoding of the given pnw value. It only minimally
 * validates the data so it can be used for further validation.
 *
 * @returns ProofInformation
 */
ProofInformation proofInformationFromPnw(const SessionContext& context, const std::string& encodedPnw)
{
    std::optional<std::string> ungzippedPnw{};

    try
    {
        const auto gzippedPnw = Base64::decodeToString(encodedPnw);
        ungzippedPnw = Deflate().decompress(gzippedPnw);
    }
    catch (const std::exception& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::Forbidden, "Failed decoding PNW data.", ex.what());
    }

    return parseProofInformation(context, *ungzippedPnw);
}

UrlArguments getTaskStatusReadyUrlArgumentsFilter(const KeyDerivation& keyDerivation)
{
    ServerRequest dummyRequest{Header{}};
    dummyRequest.setQueryParameters({{"status", "ready"}});

    UrlArguments result{{{"status", SearchParameter::Type::TaskStatus}}};
    result.parse(dummyRequest, keyDerivation);

    return result;
}

void validateProof(const SessionContext& context, const ProofContent& proofContent)
{
    A_23451.start("Validate the timestamp");
    // GEMREQ-start A_23451#validateTimestamp
    ErpExpectWithDiagnostics(
        proofContent.timestamp.has_value(), HttpStatus::Forbidden,
        "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Zeitliche Gültigkeit des "
        "Anwesenheitsnachweis überschritten).",
        "Timestamp could not be read");
    const auto& configuration = Configuration::instance();
    std::chrono::seconds proofValidity{configuration.getIntValue(ConfigurationKey::VSDM_PROOF_VALIDITY_SECONDS)};
    const auto now = context.sessionTime();
    const auto timeDifference = std::chrono::abs(*proofContent.timestamp - now);

    ErpExpect(timeDifference <= proofValidity, HttpStatus::Forbidden,
              "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Zeitliche Gültigkeit des "
              "Anwesenheitsnachweis überschritten).");
    A_23451.finish();
    // GEMREQ-end A_23451#validateTimestamp

    // GEMREQ-start A_23456#calcHmac
    SafeString hmacKey;
    try
    {
        hmacKey = context.serviceContext.getVsdmKeyCache().getKey(proofContent.keyOperatorId, proofContent.keyVersion);
    }
    catch (const std::exception& e)
    {
        TLOG(WARNING) << e.what();
        ErpFail(HttpStatus::Forbidden, "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Fehler bei "
                                       "Prüfung der HMAC-Sicherung).");
    }
    A_23454.start("extract 'prüfziffer' and calculate hash");
    A_23456.start("Use 23 leading bytes to calculate hash, compare the first 24 bytes of result");
    // the first 23 byte are the data we have to hash using the vsdm key
    const auto calculatedHash = Hash::hmacSha256(hmacKey, proofContent.validationDataForHash);
    // GEMREQ-end A_23456#calcHmac
    // GEMREQ-start A_23456#compareHmac
    std::string_view truncatedCalculatedHash{reinterpret_cast<const char*>(calculatedHash.data()),
                                             ProofInformation::truncatedHmacSize};

    if (truncatedCalculatedHash != proofContent.truncatedHmac)
    {
        TLOG(WARNING) << "VSDM PZ Validation failed for operator='" << proofContent.keyOperatorId << "' version='"
                      << proofContent.keyVersion << "'";
        ErpFail(HttpStatus::Forbidden, "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Fehler bei "
                                       "Prüfung der HMAC-Sicherung).");
    }
    // GEMREQ-end A_23456#compareHmac
    A_23456.finish();
    A_23454.finish();
}

}// namespace


GetAllTasksHandler::GetAllTasksHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : TaskHandlerBase(Operation::GET_Task, allowedProfessionOIDs)
{
}


void GetAllTasksHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    std::optional<model::Bundle> response{};

    const auto& accessToken = session.request.getAccessToken();
    if (accessToken.stringForClaim(JWT::professionOIDClaim) == profession_oid::oid_versicherter)
    {
        response = handleRequestFromInsurant(session);
    }
    else
    {
        response = handleRequestFromPharmacist(session);
    }

    A_19514.start("HttpStatus 200 for successful GET");
    makeResponse(session, HttpStatus::OK, &response.value());
    A_19514.finish();
}

model::Bundle GetAllTasksHandler::handleRequestFromInsurant(PcSessionContext& session)
{
    const auto& accessToken = session.request.getAccessToken();

    // GEMREQ-start A_19115-01
    A_19115_01.start("retrieve KVNR from ACCESS_TOKEN");
    const auto kvnrClaim = accessToken.stringForClaim(JWT::idNumberClaim);
    ErpExpect(kvnrClaim.has_value(), HttpStatus::BadRequest,
              "Missing claim in ACCESS_TOKEN: " + std::string(JWT::idNumberClaim));
    A_19115_01.finish();
    const model::Kvnr kvnr{*kvnrClaim};

    auto arguments =
        std::optional<UrlArguments>(std::in_place, std::vector<SearchParameter>{
                                                       {"status", "status", SearchParameter::Type::TaskStatus},
                                                       {"authored-on", "authored_on", SearchParameter::Type::Date},
                                                       {"modified", "last_modified", SearchParameter::Type::Date},
                                                   });
    arguments->parse(session.request, session.serviceContext.getKeyDerivation());

    auto* databaseHandle = session.database();
    A_19115_01.start("use KVNR to filter tasks");
    auto resultSet = databaseHandle->retrieveAllTasksForPatient(kvnr, arguments);
    A_19115_01.finish();
    // GEMREQ-end A_19115-01

    model::Bundle responseBundle(model::BundleType::searchset, ::model::ResourceBase::NoProfile);
    std::size_t totalSearchMatches = responseIsPartOfMultiplePages(arguments->pagingArgument(), resultSet.size())
                                         ? databaseHandle->countAllTasksForPatient(kvnr, arguments)
                                         : resultSet.size();

    const auto links = arguments->getBundleLinks(getLinkBase(), "/Task", totalSearchMatches);
    for (const auto& link : links)
    {
        responseBundle.setLink(link.first, link.second);
    }
    if (! resultSet.empty())
    {
        A_19129_01.start("create response bundle for patient");

        for (const auto& task : resultSet)
        {
            // in C_10499 the response bundle has changed to only contain tasks, no patient confirmation
            // and no receipt any longer
            responseBundle.addResource(makeFullUrl("/Task/" + task.prescriptionId().toString()), {},
                                       model::Bundle::SearchMode::match, task.jsonDocument());
        }
    }

    responseBundle.setTotalSearchMatches(totalSearchMatches);

    // No Audit data collected and no Audit event created for GET /Task (since ERP-8507).

    return responseBundle;
}

model::Bundle GetAllTasksHandler::handleRequestFromPharmacist(PcSessionContext& session)
{
    const std::optional<std::string> telematikId = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    ErpExpect(telematikId.has_value(), HttpStatus::BadRequest, "No valid Telematik-ID in JWT");

    A_23450.start("Check provided 'pnw' value");
    // GEMREQ-start A_23451#pnw
    std::optional<std::string> pnw = session.request.getQueryParameter("pnw");
    if (! pnw.has_value())
    {
        // fallback to 'PNW' for compatibility with old implementation
        pnw = session.request.getQueryParameter("PNW");
    }
    ErpExpect(pnw.has_value() && ! pnw->empty(), HttpStatus::Forbidden, "Missing or invalid PNW query parameter");

    const auto proofInformation = proofInformationFromPnw(session, *pnw);
    // GEMREQ-end A_23451#pnw

    A_23455.start("Validate presence of prüfziffer");
    ErpExpect(proofInformation.proofContent.has_value(), HttpStatus::Forbidden,
              "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Prüfziffer fehlt im VSDM "
              "Prüfungsnachweis).");
    A_23455.finish();
    const auto& proofContent = *proofInformation.proofContent;
    const auto& kvnr = proofContent.kvnr;
    ErpExpect(kvnr.valid(), HttpStatus::Forbidden, "Invalid Kvnr");
    A_23450.finish();

    // Now that we have a KVNR, start collecting the audit data
    session
        .auditDataCollector()
        // Default, will be overridden after successful decoding PNW
        .setEventId(model::AuditEventId::GET_Tasks_by_pharmacy_pnw_check_failed)
        .setAction(model::AuditEvent::Action::read)
        .setInsurantKvnr(kvnr);

    // GEMREQ-start A_23451#validate, A_23456#validate
    validateProof(session, proofContent);
    // GEMREQ-end A_23451#validate, A_23456#validate

    A_23452.start("Read tasks according to KVNR and with status 'ready'");
    const auto statusReadyFilter = getTaskStatusReadyUrlArgumentsFilter(session.serviceContext.getKeyDerivation());
    auto* database = session.database();
    auto tasks = database->retrieveAllTasksForPatient(kvnr, statusReadyFilter);
    A_23452.finish();

    for (auto& task : tasks)
    {
        const auto [taskWithAccessCode, data] = database->retrieveTaskAndPrescription(task.prescriptionId());
        task.setAccessCode(taskWithAccessCode->task.accessCode());
    }

    model::Bundle responseBundle{model::BundleType::searchset, model::ResourceBase::NoProfile};
    responseBundle.setTotalSearchMatches(tasks.size());

    for (const auto& task : tasks)
    {
        responseBundle.addResource(makeFullUrl("/Task/" + task.prescriptionId().toString()), {},
                                   model::Bundle::SearchMode::match, task.jsonDocument());
    }

    // Collect Audit data
    session.auditDataCollector().setEventId(model::AuditEventId::GET_Tasks_by_pharmacy_with_pz);

    return responseBundle;
}

GetTaskHandler::GetTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : TaskHandlerBase(Operation::GET_Task_id, allowedProfessionOIDs)
{
}

// GEMREQ-start A_21360-01#handleRequest
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
// GEMREQ-end A_21360-01#handleRequest

// GEMREQ-start A_21360-01#handleRequestFromPatient
void GetTaskHandler::handleRequestFromPatient(PcSessionContext& session, const model::PrescriptionId& prescriptionId,
                                              const JWT& accessToken)
{
    auto* databaseHandle = session.database();
    A_21532_02.start("Suppress secret for insurant (retrieveTaskAndPrescription does not read it from DB)");
    auto [taskAndKey, healthcareProviderPrescription] = databaseHandle->retrieveTaskAndPrescription(prescriptionId);
    A_21532_02.finish();
    std::optional<model::Task> task;
    if (taskAndKey.has_value())
    {
        task = std::move(taskAndKey->task);
    }

    checkTaskState(task, true);

    const auto kvnr = task->kvnr();
    Expect3(kvnr.has_value(), "Task has no KV number", std::logic_error);

    model::AuditEventId auditEventId = model::AuditEventId::GET_Task_id_insurant;

    // GEMREQ-start A_19116-01
    A_19116_01.start("check if the KVNR matches");
    const auto kvnrFromAccessToken = accessToken.stringForClaim(JWT::idNumberClaim);
    if (kvnrFromAccessToken != kvnr)
    {
        auditEventId = model::AuditEventId::GET_Task_id_representative;

        A_19116_01.finish();
        A_19116_01.start("on KVNR missmatch check if the access code matches");
        checkAccessCodeMatches(session.request, *task);
        A_19116_01.finish();

        A_20753.start("Exclusion of representative access to or using verification identity");
        if (kvnrFromAccessToken.has_value())
        {
            ErpExpect(isVerificationIdentityKvnr(kvnrFromAccessToken.value()) ==
                          isVerificationIdentityKvnr(kvnr.value().id()),
                      HttpStatus::BadRequest,
                      "KVNR verification identities may not access information from insurants and vice versa");
        }
        A_20753.finish();
    }
    // GEMREQ-end A_19116-01

    std::optional<model::KbvBundle> patientConfirmation;
    if (healthcareProviderPrescription.has_value())
    {
        task->setPatientConfirmationUuid();
        auto confirmationId = task->patientConfirmationUuid();
        Expect3(confirmationId.has_value(), "patient confirmation ID not set in task", std::logic_error);
        patientConfirmation = convertToPatientConfirmation(*healthcareProviderPrescription, Uuid(*confirmationId),
                                                           session.serviceContext);
    }

    A_20702_03.start("remove access code from task if client is unknown");
    // Unknown clients are filtered out by the VAU Proxy, nothing to do in the erp-processing-context.
    A_20702_03.finish();

    // GEMREQ-start A_21360-01#handleRequestFromPatient_deleteAccessCode
    A_21360_01.start("remove access code from task if workflow is 169 or 209");
    switch (task->type())
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            break;
        case model::PrescriptionType::direkteZuweisung:
        case model::PrescriptionType::direkteZuweisungPkv:
            task->deleteAccessCode();
            break;
    }
    A_21360_01.finish();
    // GEMREQ-end A_21360-01#handleRequestFromPatient_deleteAccessCode

    A_21375_02.start("create response bundle for patient");
    model::Bundle responseBundle(model::BundleType::collection, ::model::ResourceBase::NoProfile);
    responseBundle.setLink(model::Link::Type::Self, makeFullUrl("/Task/") + task.value().prescriptionId().toString());
    addToPatientBundle(responseBundle, task.value(), patientConfirmation, {});
    A_21375_02.finish();

    A_19569_02.start("_revinclude: reverse include referencing audit events");
    UrlArguments urlArguments({});
    urlArguments.parse(session.request, session.serviceContext.getKeyDerivation());
    if (urlArguments.hasReverseIncludeAuditEventArgument())
    {
        const auto auditDatas = databaseHandle->retrieveAuditEventData(*kvnr, {}, prescriptionId, {});
        for (const auto& auditData : auditDatas)
        {
            const auto language = getLanguageFromHeader(session.request.header())
                                      .value_or(std::string(AuditEventTextTemplates::defaultLanguage));
            const auto auditEvent = AuditEventCreator::fromAuditData(
                auditData, language, session.serviceContext.auditEventTextTemplates(), session.request.getAccessToken(),
                model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>());

            responseBundle.addResource(Uuid{auditEvent.id()}.toUrn(), {}, {}, auditEvent.jsonDocument());
        }
    }
    A_19569_02.finish();

    makeResponse(session, HttpStatus::OK, &responseBundle);

    // Collect Audit data
    session.auditDataCollector().setEventId(auditEventId).setInsurantKvnr(*kvnr);
}
// GEMREQ-end A_21360-01#handleRequestFromPatient

void GetTaskHandler::handleRequestFromPharmacist(PcSessionContext& session, const model::PrescriptionId& prescriptionId)
{
    auto* databaseHandle = session.database();
    auto [task, receipt] = databaseHandle->retrieveTaskAndReceipt(prescriptionId);

    checkTaskState(task, false);

    A_19226_01.start("create response bundle for pharmacist");
    A_20703.start("Set VAU-Error-Code header field to brute-force whenever AccessCode or Secret mismatches");
    const auto uriSecret = session.request.getQueryParameter("secret");
    VauExpect(uriSecret.has_value() && uriSecret.value() == task->secret(), HttpStatus::Forbidden,
              VauErrorCode::brute_force, "No or invalid secret provided for user pharmacy");
    A_20703.finish();

    const auto selfLink = makeFullUrl("/Task/" + task.value().prescriptionId().toString());
    model::Bundle responseBundle(model::BundleType::collection, ::model::ResourceBase::NoProfile);
    responseBundle.setLink(model::Link::Type::Self, selfLink);

    if (task->status() == model::Task::Status::completed && receipt.has_value())
    {
        task->setReceiptUuid();
        responseBundle.addResource(selfLink, {}, {}, task->jsonDocument());
        responseBundle.addResource(receipt->getId().toUrn(), {}, {}, receipt->jsonDocument());
    }
    else
    {
        responseBundle.addResource(selfLink, {}, {}, task->jsonDocument());
    }
    A_19226_01.finish();

    A_19514.start("HttpStatus 200 for successful GET");
    makeResponse(session, HttpStatus::OK, &responseBundle);
    A_19514.finish();

    // Collect Audit data
    const auto kvnr = task->kvnr();
    Expect3(kvnr.has_value(), "Task has no KV number", std::logic_error);
    session.auditDataCollector().setEventId(model::AuditEventId::GET_Task_id_pharmacy).setInsurantKvnr(*kvnr);
}

void GetTaskHandler::checkTaskState(const std::optional<model::Task>& task, bool isPatient)
{
    ErpExpect(task.has_value(), HttpStatus::NotFound, "Task not found in DB");

    if (isPatient)
    {
        ErpExpect(task->status() != model::Task::Status::draft, HttpStatus::Forbidden,
                  "Status draft is not allowed for this operation");
    }
    A_18952.start("don't return deleted mTasks");
    ErpExpect(task->status() != model::Task::Status::cancelled, HttpStatus::Gone, "Task has already been deleted");
    A_18952.finish();
}
