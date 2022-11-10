/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "TaskHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/crypto/Jws.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/Device.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/ModelException.hxx"
#include "erp/model/Signature.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/service/AuditEventCreator.hxx"
#include "erp/xml/XmlDocument.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/String.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "erp/util/Configuration.hxx"

#include <zlib.h>

#include <algorithm>
#include <cstddef>
#include <chrono>
#include <ctime>
#include <sstream>

namespace
{
    constexpr std::size_t decompressBufferSize = 2048;
    constexpr std::string_view xPathExpression_TSText = "/ns:PN/ns:TS/text()";
    constexpr std::string_view xPathExpression_EText = "/ns:PN/ns:E/text()";
    constexpr std::string_view xPathExpression_PZText = "/ns:PN/ns:PZ/text()";

    using namespace std::chrono_literals;

    /**
     * This function takes the fully-encoded PNW (gzipped XML + base64 encoded + URL encoded)
     * and performs only the URL decoding; thus its output is expected to be a base64 representation.
     */
    std::string urlDecodePnw(const std::string& encodedPnw)
    {
        std::string result{};
        result.reserve(encodedPnw.size());

        for (std::size_t itr = 0; itr < encodedPnw.size(); ++itr)
        {
            if (encodedPnw[itr] == '%')
            {
                ErpExpect(itr + 3 <= encodedPnw.size(),
                          HttpStatus::Forbidden,
                          "Failed decoding or uncompressing the PNW");

                int value{};
                std::istringstream stream{encodedPnw.substr(itr + 1, 2)};

                ErpExpect(stream >> std::hex >> value,
                          HttpStatus::Forbidden,
                          "Failed decoding or uncompressing the PNW");

                result += static_cast<char>(value);
                itr += 2;
            }
            else
            {
                result += encodedPnw[itr];
            }
        }

        return result;
    }

    /**
     * This function takes a gzipped binary representation of the PNW and inflates it.
     */
    std::string ungzipPnw(const std::string& data)
    {
        // Initialize deflation. Let zlib choose alloc and free functions.
        z_stream stream{};
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        stream.avail_in = 0;
        stream.next_in = Z_NULL;

        // Initialize zlib decompression for gzip.
        Expect(inflateInit2(&stream, 16 + MAX_WBITS) == Z_OK, "could not initialize decompression for gzip");

        // Setup `text` as input buffer.
        stream.avail_in = data.size();
        stream.next_in = const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(data.data()));
        uint8_t buffer[decompressBufferSize];

        int inflateEndResult = Z_OK;
        std::string dataResult{};

        {
            // ensure that inflateEnd() is called also when exceptions are thrown
            auto inflateFinally = gsl::finally(
                [&stream, &inflateEndResult]() { inflateEndResult = inflateEnd(&stream); });
            do
            {
                // Set up `buffer` as output buffer.
                stream.avail_out = sizeof(buffer);
                stream.next_out = buffer;

                // Inflate the next part of the input. This will either use up all remaining input or fill
                // up the output buffer.
                const int result = inflate(&stream, Z_NO_FLUSH);
                Expect(result >= Z_OK, "decompression failed");

                // Not expecting the compression to have used a training dictionary
                ErpExpect(result != Z_NEED_DICT,
                          HttpStatus::Forbidden,
                          "Failed decoding or uncompressing the PNW");

                // Append the decompressed data to the result string.
                const size_t decompressedSize = sizeof(buffer) - stream.avail_out;
                dataResult += std::string{reinterpret_cast<char*>(buffer), decompressedSize};

                // Exit when all input and output has been processed. This avoids one last
                // call to inflate().
                if (result == Z_STREAM_END)
                {
                    break;
                }
            } while (true);
        }

        // Release any dynamic memory.
        Expect(inflateEndResult == Z_OK, "finishing decompression failed");

        return dataResult;
    }

    std::vector<std::size_t> getPnwAllowedResultValues()
    {
        const auto& configuration = Configuration::instance();
        const auto allowedResultValues = configuration.getStringValue(ConfigurationKey::PNW_ALLOWED_RESULTS);

        if (allowedResultValues.empty())
        {
            return {};
        }

        const auto stringValues = String::split(allowedResultValues, ',');
        std::vector<std::size_t> integerValues(stringValues.size());
        std::transform(
            stringValues.cbegin(),
            stringValues.cend(),
            integerValues.begin(),
            [](const auto& stringValue)
            {
                std::size_t idx = 0;
                const auto pnwResultValue = std::stoul(stringValue, &idx);
                Expect(idx == stringValue.size(), "Failed to convert to number");
                return pnwResultValue;
            });

        return integerValues;
    }

    /**
     * This function validates the values of the PNW (the timestamp and the 'result') according to
     * the business logic rules: the timestamp must not be older than 30 minutes (relative to current timestamp),
     * while the 'result value' has to be one of the known configurable (known before-hand) values.
     *
     * These two (timestamp and result) are gotten from the PNW XML document by parsing it and using XPath expressions.
     * Example of how they can look like within the document:
     *
     * <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
     * <PN xmlns="http://ws.gematik.de/fa/vsdm/pnw/v1.0" CDM_VERSION="1.0.0">
     *     <TS>20220803083914</TS>
     *     <E>3</E>
     * </PN>
     *
     */
    void validatePnwData(const model::Timestamp& timestamp, const std::size_t resultValue)
    {
        const auto timestampPushedForwardHalfHour = timestamp + 30min;
        const auto currentTimestamp = model::Timestamp::now();

        ErpExpect(timestampPushedForwardHalfHour >= currentTimestamp,
                  HttpStatus::Forbidden,
                  "PNW is older than 30 minutes");

        static const auto allowedResultValues = getPnwAllowedResultValues();
        const auto findItr = std::find(allowedResultValues.cbegin(), allowedResultValues.cend(), resultValue);
        ErpExpect(allowedResultValues.cend() != findItr,
                  HttpStatus::Forbidden,
                  "Ergebnis im Prüfungsnachweis ist nicht gültig.");
    }

    std::optional<std::string> validatePnw(const std::string& pnw)
    {
        std::optional<XmlDocument> pnwDocument{};

        try
        {
            pnwDocument.emplace(pnw);
            pnwDocument->registerNamespace("ns", "http://ws.gematik.de/fa/vsdm/pnw/v1.0");
        }
        catch (const std::exception& ex)
        {
            ErpFailWithDiagnostics(
                HttpStatus::Forbidden,
                "Failed parsing PNW XML.",
                ex.what());
        }

        std::optional<model::Timestamp> pnwTimestamp{};

        try
        {
            const auto timestamp = pnwDocument->getElementText(xPathExpression_TSText.data());

            tm tm{};
            const auto* res = strptime(timestamp.c_str(), "%Y%m%d%H%M%S", &tm);
            ErpExpect(res != nullptr && *res == 0,
                      HttpStatus::Forbidden,
                      "Failed parsing TS in PNW.");

            pnwTimestamp.emplace(model::Timestamp::fromTmInUtc(tm));
        }
        catch (const std::exception& ex)
        {
            ErpFailWithDiagnostics(
                HttpStatus::Forbidden,
                "Failed parsing TS in PNW.",
                ex.what());
        }

        std::optional<std::size_t> pnwResultValue{};

        try
        {
            const auto resultValue = pnwDocument->getElementText(xPathExpression_EText.data());
            std::size_t idx = 0;
            pnwResultValue = std::stoul(resultValue, &idx);
            Expect(idx == resultValue.size(), "Failed to convert to number");
        }
        catch (const std::exception& ex)
        {
            ErpFailWithDiagnostics(
                HttpStatus::Forbidden,
                "Ergebnis im Prüfungsnachweis ist nicht gültig.",
                ex.what());
        }

        std::optional<std::string> pnwPzNumber{};

        try
        {
            pnwPzNumber = pnwDocument->getOptionalElementText(xPathExpression_PZText.data());
        }
        catch (const std::exception& ex)
        {
            ErpFailWithDiagnostics(
                HttpStatus::Forbidden,
                "Failed parsing PNW XML.",
                ex.what());
        }

        validatePnwData(*pnwTimestamp, *pnwResultValue);

        return pnwPzNumber;
    }

    /**
     * This function performs full decoding, decompression, parsing and validation of the PNW data.
     * It also returns the "PZ" data from the PNW XML, if one exists.
     */
    std::optional<std::string> decodeAndValidatePnw(const std::string& encodedPnw)
    {
        std::optional<std::string> ungzippedPnw{};

        try
        {
            const auto urlDecodedPnw = urlDecodePnw(encodedPnw);
            const auto gzippedPnw = Base64::decodeToString(urlDecodedPnw);
            ungzippedPnw = ungzipPnw(gzippedPnw);
        }
        catch (const std::exception& ex)
        {
            ErpFailWithDiagnostics(
                HttpStatus::Forbidden,
                "Failed decoding PNW data.",
                ex.what());
        }

        return validatePnw(*ungzippedPnw);
    }

    UrlArguments getTaskStatusReadyUrlArgumentsFilter(const KeyDerivation& keyDerivation)
    {
        ServerRequest dummyRequest{Header{}};
        dummyRequest.setQueryParameters({{"status", "ready"}});

        UrlArguments result{{{"status", SearchParameter::Type::TaskStatus}}};
        result.parse(dummyRequest, keyDerivation);

        return result;
    }
}

TaskHandlerBase::TaskHandlerBase(const Operation operation,
                                 const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : ErpRequestHandler(operation, allowedProfessionOIDs)
{
}

void TaskHandlerBase::addToPatientBundle(model::Bundle& bundle, const model::Task& task,
                                         const std::optional<model::KbvBundle>& patientConfirmation,
                                         const std::optional<model::Bundle>& receipt)
{
    bundle.addResource(makeFullUrl("/Task/" + task.prescriptionId().toString()),
                       {}, {}, task.jsonDocument());

    if (patientConfirmation.has_value())
    {
        bundle.addResource(patientConfirmation->getId().toUrn(), {}, {}, patientConfirmation->jsonDocument());
    }
    if (receipt.has_value())
    {
        bundle.addResource(receipt->getId().toUrn(), {}, {}, receipt->jsonDocument());
    }
}


model::KbvBundle TaskHandlerBase::convertToPatientConfirmation(const model::Binary& healthcareProviderPrescription,
                                                            const Uuid& uuid, PcServiceContext& serviceContext)
{
    A_19029_03.start("1. convert prescription bundle to JSON");
    Expect3(healthcareProviderPrescription.data().has_value(), "healthcareProviderPrescription unexpected empty Binary",
            std::logic_error);

    // read CAdES-BES signature, but do not verify it
    const CadesBesSignature cadesBesSignature =
        CadesBesSignature(std::string(*healthcareProviderPrescription.data()));

    // this comes from the database and has already been validated when initially received
    auto bundle = model::KbvBundle::fromXmlNoValidation(cadesBesSignature.payload());
    A_19029_03.finish();

    A_19029_03.start("assign identifier");
    bundle.setId(uuid);
    A_19029_03.finish();

    A_19029_03.start("2. serialize to normalized JSON");
    auto normalizedJson = bundle.serializeToCanonicalJsonString();
    A_19029_03.finish();

    A_19029_03.start("3. sign the JSON bundle using C.FD.SIG");
    JoseHeader joseHeader(JoseHeader::Algorithm::BP256R1);
    joseHeader.setX509Certificate(serviceContext.getCFdSigErp());
    joseHeader.setType(ContentMimeType::jose);
    joseHeader.setContentType(ContentMimeType::fhirJsonUtf8);

    const Jws jsonWebSignature(joseHeader, normalizedJson, serviceContext.getCFdSigErpPrv());
    const auto signatureData = jsonWebSignature.compactDetachedSerialized();
    A_19029_03.finish();

    A_19029_03.start("store the signature in the bundle");
    model::Signature signature(Base64::encode(signatureData), model::Timestamp::now(),
                               model::Device::createReferenceString(getLinkBase()));
    signature.setTargetFormat(MimeType::fhirJson);
    signature.setSigFormat(MimeType::jose);
    signature.setType(model::Signature::jwsSystem, model::Signature::jwsCode);
    bundle.setSignature(signature);
    A_19029_03.finish();

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

CadesBesSignature TaskHandlerBase::doUnpackCadesBesSignature(const std::string& cadesBesSignatureFile,
                                                             TslManager* tslManager)
{
    try
    {
        if (tslManager)
        {
            A_19025.start("verify the QES signature");
            A_20159.start("verify HBA Signature Certificate ");
            return {cadesBesSignatureFile,
                    *tslManager,
                    false,
                    {profession_oid::oid_arzt,
                     profession_oid::oid_zahnarzt,
                     profession_oid::oid_arztekammern}};
            A_20159.finish();
            A_19025.finish();
        }
        else
        {
            return CadesBesSignature(cadesBesSignatureFile);
        }
    }
    catch (const TslError& ex)
    {
        VauFail(ex.getHttpStatus(), VauErrorCode::invalid_prescription, ex.what());
    }
    catch (const CadesBesSignature::UnexpectedProfessionOidException& ex)
    {
        A_19225.start("Report 400 because of unexpected ProfessionOIDs in QES certificate.");
        VauFail(HttpStatus::BadRequest, VauErrorCode::invalid_prescription, ex.what());
        A_19225.finish();
    }
    catch (const CadesBesSignature::VerificationException& ex)
    {
        VauFail(HttpStatus::BadRequest, VauErrorCode::invalid_prescription, ex.what());
    }
    catch (const ErpException& ex)
    {
        TVLOG(1) << "ErpException: " << ex.what();
        VauFail(ex.status(), VauErrorCode::invalid_prescription, ex.what());
    }
    catch (const model::ModelException& mo)
    {
        TVLOG(1) << "ModelException: " << mo.what();
        VauFail(HttpStatus::BadRequest, VauErrorCode::invalid_prescription, "ModelException");
    }
    catch (const std::exception& ex)
    {
        VauFail(HttpStatus::InternalServerError, VauErrorCode::invalid_prescription,
                ex.what());
    }
    catch (...)
    {
        VauFail(HttpStatus::InternalServerError, VauErrorCode::invalid_prescription,
                "unexpected throwable");
    }
}

CadesBesSignature TaskHandlerBase::unpackCadesBesSignature(const std::string& cadesBesSignatureFile,
                                                           TslManager& tslManager)
{
    return doUnpackCadesBesSignature(cadesBesSignatureFile, &tslManager);
}

CadesBesSignature TaskHandlerBase::unpackCadesBesSignatureNoVerify(const std::string& cadesBesSignatureFile)
{
    return doUnpackCadesBesSignature(cadesBesSignatureFile, nullptr);
}


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

    model::Bundle responseBundle(model::BundleType::searchset, ::model::ResourceBase::NoProfile);
    std::size_t totalSearchMatches =
        responseIsPartOfMultiplePages(arguments->pagingArgument(), resultSet.size())
            ? databaseHandle->countAllTasksForPatient(kvnr.value(), arguments)
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
            Expect(task.status() != model::Task::Status::cancelled, "should already be filtered by SQL query.");
            // in C_10499 the response bundle has changed to only contain tasks, no patient confirmation
            // and no receipt any longer
            responseBundle.addResource(makeFullUrl("/Task/" + task.prescriptionId().toString()),
                                       {}, model::Bundle::SearchMode::match, task.jsonDocument());
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

    using namespace std::chrono;
    const auto short_limit = 1min;
    const auto long_limit = 24h;
    const auto timespan = time_point_cast<milliseconds>(time_point<system_clock>() + short_limit).time_since_epoch().count();
    const auto dailyTimespan =
        time_point_cast<milliseconds>(time_point<system_clock>() + long_limit).time_since_epoch().count();

    A_23161.start("Rate limit per day");
    {
        auto longValue = Configuration::instance().getIntValue(ConfigurationKey::REPORT_ALL_TASKS_RATE_LIMIT_LONG);
        RateLimiter rateLimiterDaily(session.serviceContext.getRedisClient(), "ERP-PC-DAY", longValue, dailyTimespan);
        const auto now = system_clock::now();
        auto exp = time_point_cast<seconds>(now + long_limit).time_since_epoch();
        auto exp_ms = time_point<system_clock, milliseconds>(exp);
        ErpExpect(
            rateLimiterDaily.updateCallsCounter(telematikId.value(), exp_ms),
            HttpStatus::TooManyRequests,
            "Zugriffslimit erreicht - Nächster Abruf mit eGK morgen möglich.");
    }
    A_23161.finish();

    A_23160.start("Rate limit per minute");
    {
        auto shortValue = Configuration::instance().getIntValue(ConfigurationKey::REPORT_ALL_TASKS_RATE_LIMIT_SHORT);
        RateLimiter rateLimiter(session.serviceContext.getRedisClient(), "ERP-PC-MINUTE", shortValue, timespan);
        const auto now = system_clock::now();
        auto exp = time_point_cast<seconds>(now + short_limit).time_since_epoch();
        auto exp_ms = time_point<system_clock, milliseconds>(exp);
        ErpExpect(rateLimiter.updateCallsCounter(telematikId.value(), exp_ms), HttpStatus::TooManyRequests, "Zugriffslimit erreicht - Nächster Abruf mit eGK in 1 Minute möglich.");
    }
    A_23160.finish();

    const auto kvnr = session.request.getQueryParameter("KVNR");
    ErpExpect(kvnr.has_value() && 10 == kvnr->length(),
              HttpStatus::BadRequest,
              "Missing or invalid KVNR query parameter");

    // We set audit relevant data here because we need it in case of missing or invalid PNW:
    session.auditDataCollector()
        .setEventId(model::AuditEventId::GET_Tasks_by_pharmacy_pnw_check_failed) // Default, will be overridden after successful decoding PNW;
        .setInsurantKvnr(*kvnr)
        .setAction(model::AuditEvent::Action::read);

    A_22432.start("Check provided 'PNW' value");
    const auto pnw = session.request.getQueryParameter("PNW");
    ErpExpect(pnw.has_value() && !pnw->empty(),
              HttpStatus::Forbidden,
              "Missing or invalid PNW query parameter");

    const auto pnwPzNumber = decodeAndValidatePnw(*pnw);
    A_22432.finish();

    A_22431.start("Read tasks according to KVNR and with status 'ready'");
    const auto statusReadyFilter = getTaskStatusReadyUrlArgumentsFilter(session.serviceContext.getKeyDerivation());
    auto* database = session.database();
    auto tasks = database->retrieveAllTasksForPatient(kvnr.value(), statusReadyFilter);
    A_22431.finish();

    for (auto& task : tasks) {
        const auto [taskWithAccessCode, data] = database->retrieveTaskAndPrescription(task.prescriptionId());
        task.setAccessCode(taskWithAccessCode->accessCode());
    }

    model::Bundle responseBundle{model::BundleType::searchset, model::ResourceBase::NoProfile};
    responseBundle.setTotalSearchMatches(tasks.size());

    for (const auto& task : tasks)
    {
        responseBundle.addResource(
            makeFullUrl("/Task/" + task.prescriptionId().toString()),
            {},
            model::Bundle::SearchMode::match,
            task.jsonDocument());
    }

    // Collect Audit data
    if (pnwPzNumber.has_value())
    {
        session.auditDataCollector().setEventId(model::AuditEventId::GET_Tasks_by_pharmacy_with_pz).setPnwPzNumber(*pnwPzNumber);
    }
    else
    {
        session.auditDataCollector().setEventId(model::AuditEventId::GET_Tasks_by_pharmacy_without_pz);
    }

    return responseBundle;
}

GetTaskHandler::GetTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : TaskHandlerBase(Operation::GET_Task_id, allowedProfessionOIDs)
{
}

void GetTaskHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto prescriptionId = parseId(session.request, session.accessLog);
    checkFeatureWf200(prescriptionId.type());

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

    std::optional<model::KbvBundle> patientConfirmation;
    if (healthcareProviderPrescription.has_value())
    {
        task->setPatientConfirmationUuid();
        auto confirmationId = task->patientConfirmationUuid();
        Expect3(confirmationId.has_value(), "patient confirmation ID not set in task", std::logic_error);
        patientConfirmation = convertToPatientConfirmation(*healthcareProviderPrescription, Uuid(*confirmationId),
                                                           session.serviceContext);
    }

    A_20702_01.start("remove access code from task if client is unknown");
    // Unknown clients are filtered out by the VAU Proxy, nothing to do in the erp-processing-context.
    A_20702_01.finish();

    A_21360.start("remove access code from task if workflow is 169");
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
    A_21360.finish();

    A_21375.start("create response bundle for patient");
    model::Bundle responseBundle(model::BundleType::collection, ::model::ResourceBase::NoProfile);
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

            responseBundle.addResource(Uuid{auditEvent.id()}.toUrn(), {}, {}, auditEvent.jsonDocument());
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
    auto [task, receipt] = databaseHandle->retrieveTaskAndReceipt(prescriptionId);

    checkTaskState(task);

    A_19226.start("create response bundle for pharmacist");
    A_20703.start("Set VAU-Error-Code header field to brute-force whenever AccessCode or Secret mismatches");
    const auto uriSecret = session.request.getQueryParameter("secret");
    VauExpect(uriSecret.has_value() && uriSecret.value() == task->secret(),
        HttpStatus::Forbidden, VauErrorCode::brute_force,
        "No or invalid secret provided for user pharmacy");
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
