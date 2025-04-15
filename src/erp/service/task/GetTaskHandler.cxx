/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "erp/service/task/GetTaskHandler.hxx"
#include "erp/crypto/VsdmProof.hxx"
#include "erp/model/Task.hxx"
#include "erp/util/RuntimeConfiguration.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/audit/AuditEventCreator.hxx"
#include "shared/compression/Deflate.hxx"
#include "shared/hsm/VsdmKeyCache.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Hash.hxx"
#include "shared/util/String.hxx"
#include "shared/xml/XmlDocument.hxx"
#include "erp/database/redis/TaskRateLimiter.hxx"


namespace
{
using namespace std::chrono_literals;
// the xsd can be found at https://github.com/gematik/api-telematik/blob/OPB4/fa/vsds/Pruefungsnachweis.xsd
constexpr std::string_view xPathExpression_TSText = "/ns:PN/ns:TS/text()";
constexpr std::string_view xPathExpression_EText = "/ns:PN/ns:E/text()";
constexpr std::string_view xPathExpression_PZText = "/ns:PN/ns:PZ/text()";


// GEMREQ-start proofContent
struct ProofContentV1
{
    model::Kvnr kvnr{""};
    std::optional<model::Timestamp> timestamp;
    char keyOperatorId{};
    char keyVersion{};
    std::string truncatedHmac{};
    std::string validationDataForHash;

    static ProofContentV1 deserialize(std::string_view validationData);

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

ProofContentV1 ProofContentV1::deserialize(std::string_view validationData)
{
    ProofContentV1 proofContent;
    proofContent.kvnr = model::Kvnr{validationData.substr(0, ProofContentV1::kvnrSize)};
    proofContent.keyOperatorId = validationData[ProofContentV1::posVsdmKeyOperatorId];
    proofContent.keyVersion = validationData[ProofContentV1::posVsdmKeyVersion];

    std::string_view timestampStr = validationData.substr(ProofContentV1::kvnrSize, ProofContentV1::timestampSize);
    std::uint32_t unixTimestamp{};
    const auto* endPtr = timestampStr.data() + timestampStr.size();
    auto res = std::from_chars(timestampStr.begin(), endPtr, unixTimestamp);
    if (res.ec == std::errc() && res.ptr == endPtr)
    {
        proofContent.timestamp = model::Timestamp(static_cast<int64_t>(unixTimestamp));
    }
    proofContent.truncatedHmac = validationData.substr(ProofContentV1::dataSize, ProofContentV1::truncatedHmacSize);
    proofContent.validationDataForHash = validationData.substr(0, ProofContentV1::dataSize);
    return proofContent;
}


using ProofContent = std::variant<ProofContentV1, VsdmProof2>;
struct ProofInformation {
    model::Timestamp timestamp;
    std::string result;
    // Base64 encoded data
    std::optional<std::string> proofContentBase64;
    std::optional<ProofContent> proofContent;
    static constexpr auto proofContentSizeBase64 = 64;
};


ProofContent extractProofContent(std::string_view proofContentBase64)
{
    // GEMREQ-start A_27279#pzBase64
    ErpExpect(proofContentBase64.size() == ProofInformation::proofContentSizeBase64, HttpStatus::Forbidden,
              "Invalid size of Prüfziffer");
    const auto validationDataBinary = Base64::decodeToString(proofContentBase64);
    ErpExpect(validationDataBinary.size() == ProofContentV1::dataSize + ProofContentV1::truncatedHmacSize,
              HttpStatus::Forbidden, "Invalid size of Prüfziffer");
    std::string_view validationData{validationDataBinary};
    if (validationData[0] >= 'A' && validationData[0] <= 'Z')
    {
        return ProofContentV1::deserialize(validationData);
    }
    if (static_cast<uint8_t>(validationData[0]) > 128) // NOLINT
    {
        return VsdmProof2::deserialize(validationData);
    }
    // GEMREQ-end A_27279#pzBase64
    ErpFail(HttpStatus::Forbidden, "Ungültiges Format der Prüfziffer.");
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

        const auto timestamp = pnwDocument.getElementText(std::string{xPathExpression_TSText});
        auto pzText = pnwDocument.getOptionalElementText(std::string{xPathExpression_PZText});
        std::optional<ProofContent> proofContent;
        if (pzText.has_value())
        {
            proofContent = extractProofContent(*pzText);
        }
        return ProofInformation{.timestamp = model::Timestamp::fromDtmDateTime(timestamp),
                                .result = pnwDocument.getElementText(std::string{xPathExpression_EText}),
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

void validateProofTimestamp(const SessionContext& context, std::optional<model::Timestamp> timestamp)
{
    A_23451_01.start("Validate the timestamp");
    // GEMREQ-start A_23451-01#validateTimestamp
    ErpExpectWithDiagnostics(
        timestamp.has_value(), HttpStatus::Forbidden,
        "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Zeitliche Gültigkeit des "
        "Anwesenheitsnachweis überschritten).",
        "Timestamp could not be read");
    const auto& configuration = Configuration::instance();
    std::chrono::seconds proofValidity{configuration.getIntValue(ConfigurationKey::VSDM_PROOF_VALIDITY_SECONDS)};
    const auto now = context.sessionTime();
    const auto timeDifference = std::chrono::abs(*timestamp - now);

    ErpExpect(timeDifference <= proofValidity, HttpStatus::Forbidden,
              "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Zeitliche Gültigkeit des "
              "Anwesenheitsnachweis überschritten).");
    A_23451_01.finish();
    // GEMREQ-end A_23451-01#validateTimestamp
}

void validateProofV1(SessionContext& context, const ProofContentV1& proofContent, const model::Kvnr& urlKvnr)
{
    // GEMREQ-start A_23456-01#calcHmac
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
    A_23456_01.start("Use 23 leading bytes to calculate hash, compare the first 24 bytes of result");
    // the first 23 byte are the data we have to hash using the vsdm key
    const auto calculatedHash = Hash::hmacSha256(hmacKey, proofContent.validationDataForHash);
    // GEMREQ-end A_23456-01#calcHmac
    // GEMREQ-start A_23456-01#compareHmac
    std::string_view truncatedCalculatedHash{reinterpret_cast<const char*>(calculatedHash.data()),
                                             ProofContentV1::truncatedHmacSize};

    if (truncatedCalculatedHash != proofContent.truncatedHmac)
    {
        TLOG(WARNING) << "VSDM PZ Validation failed for operator='" << proofContent.keyOperatorId << "' version='"
                      << proofContent.keyVersion << "'";
        ErpFail(HttpStatus::Forbidden, "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Fehler bei "
                                       "Prüfung der HMAC-Sicherung).");
    }
    // GEMREQ-end A_23456-01#compareHmac
    A_23456_01.finish();
    A_23454.finish();
    try
    {
        validateProofTimestamp(context, proofContent.timestamp);
    }
    catch (const std::exception&)
    {
        context.auditDataCollector()
            .setEventId(model::AuditEventId::GET_Tasks_by_pharmacy_pnw_check_failed)
            .setAction(model::AuditEvent::Action::read)
            .setInsurantKvnr(proofContent.kvnr);

        throw;
    }
    ErpExpect(urlKvnr == proofContent.kvnr, HttpStatus::KvnrMismatch, "KVNR Überprüfung fehlgeschlagen.");
}

// GEMREQ-start A_27279#decrypt
void validateProofV2(SessionContext& context, const VsdmProof2& proofContent,
                     const std::optional<std::string>& urlHcv, const model::Kvnr& urlKvnr,
                     const std::string& hashedTelematikId)
{
    SafeString sharedSecret;
    std::optional<VsdmProof2::DecryptedProof> decryptedProof;
    try
    {
        // A_27279: step 4, key exists
        sharedSecret =
            context.serviceContext.getVsdmKeyCache().getKey(proofContent.keyOperatorId(), proofContent.keyVersion());
        // A_27279: step 5: can be decrypted
        decryptedProof = proofContent.decrypt(sharedSecret);
    }
    catch (const std::exception& e)
    {
        TLOG(WARNING) << e.what();
        ErpFail(HttpStatus::Forbidden, "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (VSDM "
                                       "Schlüssel nicht vorhanden oder Inhalte nicht entschlüsselbar).");
    }
    Expect(decryptedProof.has_value(), "Decrypted proof missing");
    // GEMREQ-end A_27279#decrypt
    // GEMREQ-start A_27279#validate

    // A_27279: step 8: hcv must be equal if passed
    std::string hcv;
    if (urlHcv.has_value())
    {
        try
        {
            hcv = util::bufferToString(Base64::decode(urlHcv.value()));
        }
        catch (const std::invalid_argument& ia)
        {
            TVLOG(1) << ia.what();
            ErpFailWithDiagnostics(HttpStatus::BadRequest, "HCV Base64-Dekodierung fehlgeschlagen.", ia.what());
        }
        catch (const std::runtime_error& re)
        {
            TVLOG(1) << re.what();
            ErpFailWithDiagnostics(HttpStatus::BadRequest, "HCV Validierung fehlgeschlagen.", re.what());
        }
    }

    try {
        // A_27279: step 6: not revoked
        ErpExpect(! decryptedProof->revoked, HttpStatus::Forbidden, "eGK gesperrt");

        // A_27279: step 7: valid time
        validateProofTimestamp(context, decryptedProof->iat);
        A_27445.start("Check for RejectHcvMismatch and update counter");
        if (hcv != decryptedProof->hcv && urlHcv.has_value())
        {
            TaskRateLimiter::updateStatistic(*(context.serviceContext.getRedisClient()), hashedTelematikId);
            ErpFail(HttpStatus::RejectHcvMismatch, "HCV Validierung fehlgeschlagen.");
        }
        A_27445.finish();
    }
    catch (const std::exception&)
    {
        context.auditDataCollector()
            .setEventId(model::AuditEventId::GET_Tasks_by_pharmacy_pnw_check_failed)
            .setAction(model::AuditEvent::Action::read)
            .setInsurantKvnr(decryptedProof->kvnr);

        throw;
    }

    // A_27279: step 9: kvnr is identical to url-kvnr
    A_27445.start("Check for KvnrMismatch and update counter");
    if (urlKvnr != decryptedProof->kvnr)
    {
        TaskRateLimiter::updateStatistic(*(context.serviceContext.getRedisClient()), hashedTelematikId);
        ErpFail(HttpStatus::KvnrMismatch, "KVNR Überprüfung fehlgeschlagen.");
    }
    A_27445.finish();
    // GEMREQ-end A_27279#validate
}

}// namespace


GetAllTasksHandler::GetAllTasksHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : TaskHandlerBase(Operation::GET_Task, allowedProfessionOIDs)
{
}


void GetAllTasksHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto& accessToken = session.request.getAccessToken();
    if (accessToken.stringForClaim(JWT::professionOIDClaim) == profession_oid::oid_versicherter)
    {
        handleRequestFromInsurant(session);
    }
    else
    {
        handleRequestFromPharmacist(session);
    }
}

void GetAllTasksHandler::handleRequestFromInsurant(PcSessionContext& session)
{
    const auto& accessToken = session.request.getAccessToken();

    // GEMREQ-start A_19115-01
    A_19115_01.start("retrieve KVNR from ACCESS_TOKEN");
    const auto kvnrClaim = accessToken.stringForClaim(JWT::idNumberClaim);
    ErpExpect(kvnrClaim.has_value(), HttpStatus::BadRequest,
              "Missing claim in ACCESS_TOKEN: " + std::string(JWT::idNumberClaim));
    A_19115_01.finish();
    const model::Kvnr kvnr{*kvnrClaim};

    auto arguments = urlArgumentsForTasks();
    arguments->parse(session.request, session.serviceContext.getKeyDerivation());

    auto* databaseHandle = session.database();
    A_19115_01.start("use KVNR to filter tasks");
    auto resultSet = databaseHandle->retrieveAllTasksForPatient(kvnr, arguments);
    A_19115_01.finish();
    // GEMREQ-end A_19115-01

    model::Bundle responseBundle(model::BundleType::searchset, ::model::FhirResourceBase::NoProfile);
    std::size_t totalSearchMatches = responseIsPartOfMultiplePages(arguments->pagingArgument(), resultSet.size())
                                         ? databaseHandle->countAllTasksForPatient(kvnr, arguments)
                                         : resultSet.size();

    const auto links = arguments->createBundleLinks(getLinkBase(), "/Task", totalSearchMatches);
    for (const auto& link : links)
    {
        responseBundle.setLink(link.first, link.second);
    }
    if (! resultSet.empty())
    {
        A_19129_01.start("create response bundle for patient");

        for (auto& task : resultSet)
        {
            task.removeLastMedicationDispenseConditional();
            // in C_10499 the response bundle has changed to only contain tasks, no patient confirmation
            // and no receipt any longer
            responseBundle.addResource(makeFullUrl("/Task/" + task.prescriptionId().toString()), {},
                                       model::Bundle::SearchMode::match, task.jsonDocument());
        }
    }

    responseBundle.setTotalSearchMatches(totalSearchMatches);

    // No Audit data collected and no Audit event created for GET /Task (since ERP-8507).
    A_19514.start("HttpStatus 200 for successful GET");
    makeResponse(session, HttpStatus::OK, &responseBundle);
    A_19514.finish();
}

void GetAllTasksHandler::handleRequestFromPharmacist(PcSessionContext& session)
{
    const auto& config = Configuration::instance();
    const std::optional<std::string> telematikId = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    ErpExpect(telematikId.has_value(), HttpStatus::BadRequest, "No valid Telematik-ID in JWT");
    const auto hashedTelematikId = session.serviceContext.getKeyDerivation().hashTelematikId(model::TelematikId{telematikId.value()}).toHex();

    A_27446.start("Check for exceeded access counter");
    ErpExpect(! TaskRateLimiter::isBlockedForToday(*(session.serviceContext.getRedisClient()), hashedTelematikId),
              HttpStatus::Locked, "TelematikId blocked for today.");
    A_27446.finish();

    A_25208_01.start("Verify given kvnr parameter is given");
    const auto kvnrQueryValue = session.request.getQueryParameter("kvnr");
    ErpExpect(kvnrQueryValue.has_value(), HttpStatus::RejectMissingKvnr,
              "Ein Prüfnachweis ohne KVNR wird nicht akzeptiert.");
    A_25208_01.finish();
    const model::Kvnr kvnr = model::Kvnr{kvnrQueryValue.value()};
    ErpExpect(kvnr.validFormat(), HttpStatus::Forbidden, "Invalid Kvnr");

    // GEMREQ-start A_27279#enforceHcvCheck
    A_27346.start("Read hcv query parameter");
    const auto hcvQueryValue = session.request.getQueryParameter("hcv");
    ErpExpect(! config.getBoolValue(ConfigurationKey::SERVICE_TASK_GET_ENFORCE_HCV_CHECK) || hcvQueryValue.has_value(),
              HttpStatus::RejectMissingHcv, "Ein Prüfnachweis ohne HCV wird nicht akzeptiert.");
    A_25208_01.finish();
    // GEMREQ-end A_27279#enforceHcvCheck

    A_23450.start("Check provided 'pnw' value");
    // GEMREQ-start A_23451-01#pnw
    std::optional<std::string> pnw = session.request.getQueryParameter("pnw");
    if (! pnw.has_value())
    {
        // fallback to 'PNW' for compatibility with old implementation
        pnw = session.request.getQueryParameter("PNW");
    }
    ErpExpect(pnw.has_value() && ! pnw->empty(), HttpStatus::Forbidden, "Missing or invalid PNW query parameter");

    const auto proofInformation = proofInformationFromPnw(session, *pnw);
    // GEMREQ-end A_23451-01#pnw

    A_25206.start("Validate result equal 3");
    ErpExpect(proofInformation.proofContent.has_value() || proofInformation.result == "3", HttpStatus::Forbidden,
              "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Prüfziffer fehlt im VSDM "
              "Prüfungsnachweis oder ungültiges Ergebnis im Prüfungsnachweis).");
    A_25206.finish();

    // GEMREQ-start A_23451-01#proofContent, A_23456-01##proofContent
    if (proofInformation.proofContent.has_value())
    // GEMREQ-end A_23451-01#proofContent, A_23456-01##proofContent
    {
        const auto& proofContent = *proofInformation.proofContent;
        // GEMREQ-start A_23451-01#validate, A_23456-01#validate
        struct ProofValidator {
            void operator()(const ProofContentV1& proof)
            {
                validateProofV1(session, proof, kvnr);
            };
            void operator()(const VsdmProof2& proof)
            {
                validateProofV2(session, proof, hcv, kvnr, hashedTelematikId);
            };
            PcSessionContext& session; // NOLINT(misc-non-private-member-variables-in-classes)
            const std::optional<std::string>& hcv; // NOLINT(misc-non-private-member-variables-in-classes)
            const model::Kvnr& kvnr; // NOLINT(misc-non-private-member-variables-in-classes)
            const std::string hashedTelematikId; // NOLINT(misc-non-private-member-variables-in-classes)
        };

        std::visit(ProofValidator{session, hcvQueryValue, kvnr, hashedTelematikId}, proofContent);
        // GEMREQ-end A_23451-01#validate, A_23456-01#validate

        session
            .auditDataCollector()
            .setEventId(model::AuditEventId::GET_Tasks_by_pharmacy_with_pz)
            .setAction(model::AuditEvent::Action::read)
            .setInsurantKvnr(kvnr);

        const auto response = handleRequestFromPharmacist(session, kvnr);

        A_19514.start("HttpStatus 200 for successful GET");
        makeResponse(session, HttpStatus::OK, &response);
        A_19514.finish();
    }
    else
    {
        const auto runtimeConfig = session.serviceContext.getRuntimeConfigurationGetter();
        const auto isAcceptPN3 = runtimeConfig->isAcceptPN3Enabled();

        session
            .auditDataCollector()
            .setEventId(isAcceptPN3?
                model::AuditEventId::GET_Tasks_by_pharmacy_with_pn3:
                model::AuditEventId::GET_Tasks_by_pharmacy_with_pn3_failed
            )
            .setAction(model::AuditEvent::Action::read)
            .setInsurantKvnr(kvnr);

        A_25207.start("Accept PN3 disabled");
        ErpExpect(isAcceptPN3, HttpStatus::NotAcceptPN3,
            "Es wird kein Prüfnachweis mit Ergebnis 3 (ohne Prüfziffer) akzeptiert.");
        A_25207.finish();

        const auto acceptPN3Expiry = runtimeConfig->getAcceptPN3Expiry();
        if(acceptPN3Expiry <= model::Timestamp::now())
        {
            TLOG(ERROR) << "GET Task successfully called with a PN3, but AcceptPN3 expired at "
                    << acceptPN3Expiry.toXsDateTime();
        }

        const auto response = handleRequestFromPharmacist(session, kvnr);

        A_25209.start("HttpStatus 202 for successful GET");
        makeResponse(session, HttpStatus::Accepted, &response);
        A_25209.finish();
    }
    A_23450.finish();
}

model::Bundle GetAllTasksHandler::handleRequestFromPharmacist(PcSessionContext& session, const model::Kvnr& kvnr)
{
    A_25209.start("Read tasks according to KVNR and with status 'ready'");
    A_23452_02.start("Read tasks according to KVNR and with status 'ready'");
    // GEMREQ-start A_23452-02#retrieveAllTasksForPatient
    std::vector<std::pair<std::string, std::string>> queryParameters
        {{"status", "ready"},{"expiry-date", "ge" + model::Timestamp::now().toGermanDate()}};
    for (const auto& pair : session.request.getQueryParameters())
    {
        if (pair.first != "pnw" || pair.first != "PNW" || pair.first != "kvnr")
        {
            queryParameters.emplace_back(pair.first, pair.second);
        }
    }

    auto arguments = urlArgumentsForTasks();
    arguments->parse(queryParameters, session.serviceContext.getKeyDerivation());

    auto* database = session.database();
    auto tasks = database->retrieveAll160TasksWithAccessCode(kvnr, arguments);
    // GEMREQ-end A_23452-02#retrieveAllTasksForPatient
    A_23452_02.finish();
    A_25209.finish();

    model::Bundle responseBundle{model::BundleType::searchset, model::FhirResourceBase::NoProfile};
    std::size_t totalSearchMatches = responseIsPartOfMultiplePages(arguments->pagingArgument(), tasks.size())
                                         ? database->countAll160Tasks(kvnr, arguments)
                                         : tasks.size();

    auto argumentsResp = urlArgumentsForTasks({
        {"pnw", "pnw", SearchParameter::Type::String},
        {"PNW", "PNW", SearchParameter::Type::String},
        {"kvnr", "kvnr", SearchParameter::Type::String},
        {"hcv", "hcv", SearchParameter::Type::String},
    });
    argumentsResp->parse(session.request.getQueryParameters(), session.serviceContext.getKeyDerivation());
    const auto links = argumentsResp->createBundleLinks(getLinkBase(), "/Task", totalSearchMatches);
    for (const auto& link : links)
    {
        responseBundle.setLink(link.first, link.second);
    }
    responseBundle.setTotalSearchMatches(totalSearchMatches);

    for (auto& task : tasks)
    {
        task.removeLastMedicationDispenseConditional();
        responseBundle.addResource(makeFullUrl("/Task/" + task.prescriptionId().toString()), {},
                                   model::Bundle::SearchMode::match, task.jsonDocument());
    }

    return responseBundle;
}

std::optional<UrlArguments> GetAllTasksHandler::urlArgumentsForTasks(
    const std::vector<SearchParameter>& searchParamsAddon)
{
    std::vector<SearchParameter> searchParams {
        {"status", "status", SearchParameter::Type::TaskStatus},
        {"authored-on", "authored_on", SearchParameter::Type::Date},
        {"modified", "last_modified", SearchParameter::Type::Date},
        {"expiry-date", "expiry_date", SearchParameter::Type::SQLDate},
        {"accept-date", "accept_date", SearchParameter::Type::SQLDate}
    };
    for (const auto& searchParameter : searchParamsAddon)
    {
        searchParams.push_back(searchParameter);
    }

    A_24438.start("Set authored-on as default sort argument.");
    std::optional<UrlArguments> arguments = UrlArguments(std::move(searchParams), "authored-on");
    A_24438.finish();
    return arguments;
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
        handleRequestFromPharmacist(session, prescriptionId, accessToken);
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

        A_26148.start("Representative may not retrieve Task with Flowtype 169/209");
        ErpExpect(!model::isDirectAssignment(task->type()), HttpStatus::Forbidden,
                  "Insurant representatives may not access directly assigned tasks");
        A_26148.finish();
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
        case model::PrescriptionType::digitaleGesundheitsanwendungen:
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
    model::Bundle responseBundle(model::BundleType::collection, ::model::FhirResourceBase::NoProfile);
    responseBundle.setLink(model::Link::Type::Self, makeFullUrl("/Task/") + task.value().prescriptionId().toString());
    task->removeLastMedicationDispenseConditional();
    addToPatientBundle(responseBundle, task.value(), patientConfirmation, {});
    A_21375_02.finish();

    A_19569_03.start("_revinclude: reverse include referencing audit events");
    UrlArguments urlArguments({});
    urlArguments.parse(session.request, session.serviceContext.getKeyDerivation());
    if (urlArguments.hasReverseIncludeAuditEventArgument())
    {
        const auto auditDatas = databaseHandle->retrieveAuditEventData(*kvnr, {}, prescriptionId, {});
        for (const auto& auditData : auditDatas)
        {
            const auto language = getLanguageFromHeader(session.request.header())
                                      .value_or(std::string(AuditEventTextTemplates::defaultLanguage));
            const auto auditEvent =
                AuditEventCreator::fromAuditData(auditData, language, session.serviceContext.auditEventTextTemplates(),
                                                 session.request.getAccessToken());

            responseBundle.addResource(Uuid{auditEvent.id()}.toUrn(), {}, {}, auditEvent.jsonDocument());
        }
    }
    A_19569_03.finish();

    makeResponse(session, HttpStatus::OK, &responseBundle);

    // Collect Audit data
    session.auditDataCollector().setEventId(auditEventId).setInsurantKvnr(*kvnr);
}
// GEMREQ-end A_21360-01#handleRequestFromPatient

void GetTaskHandler::handleRequestFromPharmacist(PcSessionContext& session, const model::PrescriptionId& prescriptionId,
                                                 const JWT& accessToken)
{
    auto* databaseHandle = session.database();

    std::optional<model::Task> task;
    std::optional<model::Bundle> receipt;
    std::optional<model::Binary> prescriptionBinary;

    // GEMREQ-start A_24176#Call
    // GEMREQ-start A_24177#Call
    // GEMREQ-start A_24178
    if (const auto uriSecret = session.request.getQueryParameter("secret"))
    {
        std::tie(task, receipt) = databaseHandle->retrieveTaskAndReceipt(prescriptionId);
        checkTaskState(task, false);
        A_20703.start("Set VAU-Error-Code header field to brute-force whenever AccessCode or Secret mismatches");
        VauExpect(uriSecret.value() == task->secret(), HttpStatus::Forbidden,
                    VauErrorCode::brute_force, "No or invalid secret provided for user pharmacy");
        A_20703.finish();
    }
    else
    // GEMREQ-start A_24179#loadFromDB
    {
        // call to `/Task/<id>?ac=...` --> SecretRecovery
        ErpExpect(getAccessCode(session.request).has_value(), HttpStatus::Forbidden,
                  "Neither AccessCode(ac) nor secret provided as URI-Parameter.");
        session.addOuterResponseHeaderField(Header::InnerRequestOperation,
                                            std::string(toString(getOperation())) + "?ac");
        std::tie(task, prescriptionBinary) = databaseHandle->retrieveTaskWithSecretAndPrescription(prescriptionId);
        checkTaskState(task, false);
        A_24177.start("check AccessCode");
        checkAccessCodeMatches(session.request, value(task));
        A_24177.finish();
        A_24178.start("only allow secrect recovery for Tasks that are in-progress");
        ErpExpect(value(task).status()== model::Task::Status::inprogress, HttpStatus::PreconditionFailed,
                  "Task not in-progress.");
        A_24178.finish();
        checkPharmacyIsOwner(value(task), accessToken);
        task->setHealthCarePrescriptionUuid();
    }
    // GEMREQ-end A_24179#loadFromDB
    // GEMREQ-end A_24178
    // GEMREQ-end A_24177#Call
    // GEMREQ-end A_24176#Call
    task->deleteAccessCode();

    A_19226_01.start("create response bundle for pharmacist");
    const auto selfLink = makeFullUrl("/Task/" + task.value().prescriptionId().toString());
    model::Bundle responseBundle(model::BundleType::collection, ::model::FhirResourceBase::NoProfile);
    responseBundle.setLink(model::Link::Type::Self, selfLink);

    if (task->status() == model::Task::Status::completed && receipt.has_value())
    {
        task->setReceiptUuid();
    }
    else
    {
        receipt.reset();
    }
    task->removeLastMedicationDispenseConditional();
    responseBundle.addResource(selfLink, {}, {}, task->jsonDocument());
    // GEMREQ-start A_24179#addToBundle
    if (prescriptionBinary)
    {
        auto fullUrl = std::string{"urn:uuid:"}.append(value(task->healthCarePrescriptionUuid()));
        responseBundle.addResource(fullUrl, {}, {}, prescriptionBinary->jsonDocument());
    }
    // GEMREQ-end A_24179#addToBundle
    if (receipt)
    {
        responseBundle.addResource(receipt->getId().toUrn(), {}, {}, receipt->jsonDocument());
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

// GEMREQ-start A_24176#checkPharmacyIsOwner
void GetTaskHandler::checkPharmacyIsOwner(const model::Task& task, const JWT& accessToken)
{
    A_24176.start("compare Task.owner to idNumberClaim in ACCESS_TOKEN");
    auto telematikId = accessToken.stringForClaim(JWT::idNumberClaim);
    ErpExpect(telematikId.has_value(), HttpStatus::BadRequest, "Missing Telematik-ID in ACCESS_TOKEN");
    const auto& owner = task.owner();
    ErpExpect(owner && *owner == *telematikId, HttpStatus::PreconditionFailed,
              "Task.owner doesn't match idNumberClaim in ACCESS_TOKEN");
    A_24176.finish();
}
// GEMREQ-end A_24176#checkPharmacyIsOwner
