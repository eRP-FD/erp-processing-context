/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/VauRequestHandler.hxx"
#include "erp/database/Database.hxx"
#include "erp/database/redis/RateLimiter.hxx"
#include "erp/model/OuterResponseErrorData.hxx"
#include "erp/pc/pre_user_pseudonym/PreUserPseudonym.hxx"
#include "erp/pc/pre_user_pseudonym/PreUserPseudonymManager.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/server/response/ResponseBuilder.hxx"
#include "erp/service/ErpRequestHandler.hxx"
#include "erp/tee/ErpTeeProtocol.hxx"
#include "erp/tee/InnerTeeRequest.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/AesGcm.hxx"
#include "shared/crypto/CMAC.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/hsm/HsmException.hxx"
#include "shared/model/Device.hxx"
#include "shared/model/MedicationDispenseId.hxx"
#include "shared/model/OperationOutcome.hxx"
#include "shared/model/ProfessionOid.hxx"
#include "shared/tsl/error/TslError.hxx"
#include "shared/util/Demangle.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/JwtException.hxx"
#include "shared/util/TLog.hxx"

#include <boost/exception/diagnostic_information.hpp>
#include <pqxx/except>
#include <typeinfo>

namespace
{

void storeAuditData(PcSessionContext& sessionContext, const JWT& accessToken)
{
    A_19391_01.start("Use name of caller for audit logging");
    A_19392.start("Use id of caller for audit logging");
    sessionContext.auditDataCollector().fillFromAccessToken(accessToken);
    A_19391_01.finish();
    A_19392.finish();

    sessionContext.auditDataCollector().setDeviceId(model::Device::Id);
    try
    {
        model::AuditData auditData = sessionContext.auditDataCollector().createData();
        // Store in database
        const auto id = sessionContext.database()->storeAuditEventData(auditData);
        TVLOG(1) << "AuditEvent record with id " << id << " created";
    }
    catch(const MissingAuditDataException& exc)
    {
        TLOG(WARNING) << "Missing Audit relevant data, unable to create Audit data record";
        sessionContext.accessLog.locationFromException(exc);
        sessionContext.accessLog.error("Missing Audit relevant data, unable to create Audit data record");
        throw;
    }
    catch(const std::exception& exc)
    {
        TLOG(WARNING) << "Error while storing Audit data";
        const auto typeinfo = util::demangle(typeid(exc).name());
        TVLOG(1) << "Error reason:  " << exc.what() << " (" << typeinfo << ")";
        sessionContext.accessLog.locationFromException(exc);
        sessionContext.accessLog.error(std::string("Error while storing Audit data (") + typeinfo + ")");
        throw;
    }
    catch(...)
    {
        TLOG(WARNING) << "Unknown error while storing Audit data";
        sessionContext.accessLog.error("Unknown error while storing Audit data");
        throw;
    }
}

JWT getJwtFromAuthorizationHeader(const std::string& authorizationHeaderValue)
{
    ErpExpect(String::starts_with( authorizationHeaderValue , "Bearer "), HttpStatus::BadRequest, "invalid Authorization Bearer header field");
    return JWT(authorizationHeaderValue.substr(7));
}

void fillErrorResponse(ServerResponse& innerResponse,
                       const HttpStatus httpStatus,
                       const std::unique_ptr<ServerRequest>& innerRequest,
                       const model::OperationOutcome& operationOutcome)
{
    ResponseBuilder(innerResponse).status(httpStatus).clearBody().keepAlive(false);
    bool callerWantsJson = false;
    if(innerRequest)
    {
        // Read wanted format from inner request if available, else use XML as general default;
        try
        {
            callerWantsJson = ErpRequestHandler::callerWantsJson(*innerRequest);
        }
        catch(...)
        {   // use default for specific role:
            const auto professionOIDClaim = innerRequest->getAccessToken().stringForClaim(JWT::professionOIDClaim);
            callerWantsJson = professionOIDClaim.has_value() && professionOIDClaim == profession_oid::oid_versicherter;
        }
    }
    ResponseBuilder(innerResponse).body(callerWantsJson, operationOutcome);
}

void fillErrorResponse(ServerResponse& innerResponse,
                       const HttpStatus httpStatus,
                       const std::unique_ptr<ServerRequest>& innerRequest,
                       const std::string& detailsText,
                       const std::optional<std::string>& diagnostics)
{
    // By now issue type, error text and diagnostics (if available) are filled.
    const model::OperationOutcome operationOutcome({
        model::OperationOutcome::Issue::Severity::error,
        model::OperationOutcome::httpCodeToOutcomeIssueType(httpStatus),
        detailsText, {}, diagnostics, {} /*expression*/ });
    fillErrorResponse(innerResponse, httpStatus, innerRequest, operationOutcome);
}

} // anonymous namespace


namespace
{
void runErpExceptionHandler(const ErpException& exception,
                            const std::unique_ptr<ServerRequest>& innerRequest,
                            ServerResponse& innerResponse, PcSessionContext& outerSession)
{
    using namespace std::string_literals;
    TVLOG(1) << "caught ErpException what=" << exception.what()
             << " diagnostics=" << exception.diagnostics().value_or("none");
    outerSession.accessLog.locationFromException(exception);
    std::string detailsText = exception.what();
    std::optional<std::string> diagnostics = exception.diagnostics();
    switch (exception.status())
    {
        case HttpStatus::BadRequest:
        case HttpStatus::BackendCallFailed:
            outerSession.accessLog.error("ErpException: " + detailsText);
            break;
        case HttpStatus::InternalServerError: // fixed text and no diagnostics for internal errors to avoid leaking of personal information;
            detailsText = "Internal server error.";
            diagnostics = {};
            [[fallthrough]];
        default:
            outerSession.accessLog.error("ErpException"s);
    }
    fillErrorResponse(innerResponse, exception.status(), innerRequest, detailsText, diagnostics);
    if (exception.vauErrorCode().has_value())
    {
        A_20703.start("Set VAU-Error-Code header field to brute_force whenever AccessCode or Secret mismatches");
        A_20704.start("Set VAU-Error-Code header field to invalid_prescription when an invalid prescription has been "
                      "transmitted");
        outerSession.response.setHeader(Header::VAUErrorCode, std::string{vauErrorCodeStr(*exception.vauErrorCode())});
        A_20704.finish();
        A_20703.finish();
    }
}

void erpServiceExceptionHandler(const ErpServiceException& exception,
                                const std::unique_ptr<ServerRequest>& innerRequest,
                                ServerResponse& innerResponse, PcSessionContext& outerSession)
{
    using namespace std::string_literals;
    TVLOG(1) << "caught ErServiceException what=" << exception.what()
         << " OperationOutcome=" << exception.operationOutcome().serializeToJsonString();
    outerSession.accessLog.locationFromException(exception);
    switch (exception.status())
    {
        case HttpStatus::BadRequest:
        case HttpStatus::BackendCallFailed:
            outerSession.accessLog.error("ErpServiceException: " + exception.operationOutcome().concatDetails());
            fillErrorResponse(innerResponse, exception.status(), innerRequest, exception.operationOutcome());
            break;
        case HttpStatus::InternalServerError:
            // fixed text and no diagnostics for internal errors to avoid leaking of personal information;
            fillErrorResponse(innerResponse, exception.status(), innerRequest, "Internal server error.", {});
            break;
        default:
            outerSession.accessLog.error("ErpServiceException"s);
            fillErrorResponse(innerResponse, exception.status(), innerRequest, exception.operationOutcome());
            break;
    }
    if (exception.vauErrorCode().has_value())
    {
        A_20703.start("Set VAU-Error-Code header field to brute_force whenever AccessCode or Secret mismatches");
        A_20704.start("Set VAU-Error-Code header field to invalid_prescription when an invalid prescription has been "
            "transmitted");
        outerSession.response.setHeader(Header::VAUErrorCode, std::string{vauErrorCodeStr(*exception.vauErrorCode())});
        A_20704.finish();
        A_20703.finish();
    }
}


void runJwtExceptionHandler(const JwtException& exception,
                            const std::unique_ptr<ServerRequest>& innerRequest,
                            ServerResponse& innerResponse,
                            PcSessionContext& outerSession,
                            const std::string& errorText,
                            HttpStatus httpStatus,
                            const std::string& headerField, const std::string& headerValue)
{
    TVLOG(1) << exception.what();
    if (!headerField.empty() && !headerValue.empty())
    {
        innerResponse.setHeader(headerField, headerValue);
    }
    outerSession.accessLog.locationFromException(exception);
    outerSession.accessLog.error(errorText);
    fillErrorResponse(innerResponse, httpStatus, innerRequest, errorText, {}/*diagnostics*/);
}


void runJwtInvalidRfcFormatExceptionHandler(const JwtInvalidRfcFormatException& exception,
                                            const std::unique_ptr<ServerRequest>& innerRequest,
                                            ServerResponse& innerResponse,
                                            PcSessionContext& outerSession)
{
    TVLOG(1) << exception.what();
    std::string errorText("JWT has invalid RFC format");
    std::optional<std::string> externalInterface;
    if (innerRequest->header().hasHeader(Header::ExternalInterface))
    {
        externalInterface = innerRequest->header().header(Header::ExternalInterface);
    }
    if (externalInterface.has_value() && externalInterface.value() == Header::ExternalInterface_TI)
    {
        A_19130.start("Invalid format from a 'lei' request");
        innerResponse.setHeader(Header::WWWAuthenticate,
                                std::string{VauRequestHandler::wwwAuthenticateErrorTiRequest()});
        errorText += " ('lei' request)";
        A_19130.finish();
    }
    else if (externalInterface.has_value() && externalInterface.value() == Header::ExternalInterface_INTERNET)
    {
        A_19389.start("Invalid format from a 'vers' request");
        innerResponse.setHeader(Header::WWWAuthenticate,
                                std::string{VauRequestHandler::wwwAuthenticateErrorInternetRequest()});
        errorText += " ('vers' request)";
        A_19389.finish();
    }
    else
    {
        innerResponse.setHeader(Header::WWWAuthenticate,
                                std::string{VauRequestHandler::wwwAuthenticateErrorInvalidToken()});
    }

    outerSession.accessLog.error(errorText);
    fillErrorResponse(innerResponse, HttpStatus::Unauthorized, innerRequest, errorText, {}/*diagnostics*/);
}

template<typename PqxxException>
void runPqxxExceptionHandler(const PqxxException& e, const std::unique_ptr<ServerRequest>& innerRequest,
                             ServerResponse& innerResponse, AccessLog& accessLog)
{
    const std::string typeinfo = util::demangle(typeid(e).name());
    TVLOG(1) << "caught pqxx exception (" << typeinfo << ") " << e.what();
    ResponseBuilder(innerResponse).status(HttpStatus::InternalServerError).clearBody().keepAlive(false);
    accessLog.locationFromException(e);
    accessLog.error(typeinfo + ": " + e.what());
    fillErrorResponse(innerResponse, HttpStatus::InternalServerError, innerRequest, "internal server error", "");
}

} // namespace exception_handlers


VauRequestHandler::VauRequestHandler(RequestHandlerManager&& handlers)
    : mRequestHandlers(std::move(handlers))
{
}


void VauRequestHandler::handleRequest(BaseSessionContext& baseSessionContext)
{
    auto& session = dynamic_cast<SessionContext&>(baseSessionContext);
    const auto sessionIdentifier = session.request.header().header(Header::XRequestId).value_or("unknown X-Request-Id");

    // Set up the duration consumer that will log spend time and call back
    DurationConsumerGuard durationConsumerGuard(
        sessionIdentifier,
        [&session](const std::chrono::steady_clock::duration duration, const std::string& category,
                           const std::string& /*description*/, const std::string& /*sessionIdentifier*/,
                           const std::unordered_map<std::string, std::string>& /*keyValueMap*/,
                           const std::optional<JsonLog::LogReceiver>& /*logReceiverOverride*/) {
            const auto durationMusecs = std::chrono::duration_cast<std::chrono::microseconds>(duration);
            if (category == DurationConsumer::categoryOcspRequest)
            {
                session.backendDuration += durationMusecs;
            }
        });

    session.accessLog.keyValue("health", session.serviceContext.applicationHealth().isUp() ? "UP" : "DOWN");

    HttpStatus errorStatus = HttpStatus::OK;
    std::string errorText;
    std::optional<std::string> errorMessage;
    try {
        auto upParam = session.request.getPathParameter("UP");
        Expect3(upParam.has_value(), "Missing Pre-User-Pseudonym in Path.", std::logic_error);

        auto innerTeeRequest = std::make_unique<InnerTeeRequest>(
            ErpTeeProtocol::decrypt(session.request.getBody(), session.serviceContext.getHsmPool()));
        handleInnerRequest(session, upParam.value(), std::move(innerTeeRequest));
        return;
    }
    catch (const AesGcmException& e)
    {
        // Can't encrypt/decrypt. Only outer response can be provided.
        session.accessLog.locationFromException(e);
        errorStatus = HttpStatus::BadRequest;
        errorText = std::string("vau decryption failed: AesGcmException ") + e.what();
    }
    catch (const ErpException& e)
    {
        session.accessLog.locationFromException(e);
        errorStatus = e.status();
        errorText = std::string("vau decryption failed: ErpException ") + e.what();
        errorMessage = e.diagnostics();
    }
    catch (const JwtException& e)
    {
        session.accessLog.locationFromException(e);
        errorStatus = HttpStatus::Unauthorized;
        errorText = std::string("vau decryption failed: JwtException ") + e.what();
    }
    catch (const HsmException& e)
    {
        session.accessLog.locationFromException(e);
        errorStatus = HttpStatus::InternalServerError;
        errorText = std::string("vau decryption failed: HsmException ") + e.what();
    }
    catch (const std::exception& e)
    {
        session.accessLog.locationFromException(e);
        errorStatus = HttpStatus::InternalServerError;
        const auto typeinfo = util::demangle(typeid(e).name());
        errorText = std::string("vau decryption failed: std::exception (") + typeinfo + ")";
        TVLOG(1) << errorText << ": " << e.what();
    }
    catch (...)
    {
        errorStatus = HttpStatus::InternalServerError;
        errorText = "vau decryption failed: unexpected exception";
    }

    model::OuterResponseErrorData errorData(sessionIdentifier, errorStatus, errorText, errorMessage);
    ResponseBuilder(session.response)
        .status(errorStatus)
        .body(true, errorData)
        .header(Header::ContentType, ContentMimeType::json)
        .keepAlive(false);
    session.accessLog.error(errorText);
}


void VauRequestHandler::handleInnerRequest(PcSessionContext& outerSession,
                                           const std::string& upParam,
                                           std::unique_ptr<InnerTeeRequest> innerTeeRequest)
{
    std::unique_ptr<ServerRequest> innerServerRequest;
    ServerResponse innerServerResponse;
    // Remove when all endpoints are implemented (or update missing endpoint return values.)
    innerServerResponse.setStatus(HttpStatus::OK);
    Operation innerOperation = Operation::UNKNOWN;

    try {
        innerTeeRequest->parseHeaderAndBody();
        innerServerRequest = std::make_unique<ServerRequest>( innerTeeRequest->releaseHeader() );
        innerServerRequest->setBody(innerTeeRequest->releaseBody());

        ErpExpect(innerServerRequest->header().hasHeader(Header::Authorization),
                  HttpStatus::BadRequest, "Authorization header is missing");
        const JWT vauJwt = innerTeeRequest->releaseAuthenticationToken();
        JWT authorizationJwt = getJwtFromAuthorizationHeader(innerServerRequest->header().header(Header::Authorization).value()) ;
        ErpExpect(authorizationJwt == vauJwt,
                  HttpStatus::BadRequest, "Authorization header is invalid");
        // NOLINTNEXTLINE(hicpp-move-const-arg,performance-move-const-arg)
        innerServerRequest->setAccessToken(std::move(authorizationJwt));

        // Create an inner session that contains the unencrypted request and a, yet to be filled, unencrypted response.
        PcSessionContext innerSession(outerSession.serviceContext, *innerServerRequest, innerServerResponse,
                                      outerSession.accessLog, outerSession.sessionTime());

        // Look up the secondary request handler. Required for determining the inner operation.
        const std::string& target = innerServerRequest->header().target();
        auto matchingHandler =
            mRequestHandlers.findMatchingHandler(innerServerRequest->header().method(), target);
        if (matchingHandler.handlerContext == nullptr)
        {
            TVLOG(1) << "did not find a handler for " << innerServerRequest->header().method() << " "
                     << target;
            A_19030.start("return 405 if no handler for Task with method and target was found");
            A_19400.start("return 405 if no handler for MedicationDispense with method and target was found");
            A_19401.start("return 405 if no handler for Communication with method and target was found");
            A_19402.start("return 405 if no handler for AuditEvent with method and target was found");
            A_22111.start("return 405 if no handler for ChargeItem with method and target was found");
            A_22153.start("return 405 if no handler for Consent with method and target was found");
            ErpFail(HttpStatus::MethodNotAllowed, "no matching handler found.");
        }

        // Determine inner operation. Required for final response.
        auto* handler = matchingHandler.handlerContext->handler.get();
        innerOperation = (handler != nullptr) ? handler->getOperation() : Operation::UNKNOWN;
        outerSession.accessLog.setInnerRequestOperation(toString(innerOperation));

        // GEMREQ-start A_19439, A_20373, A_20365
        A_20163.start("4 - verify JWT");
        A_20365_01.start("Pass the IDP pubkey to the verification method.");
        innerServerRequest->getAccessToken().verify(getIdpPublicKey(outerSession.serviceContext));
        A_20365_01.finish();
        A_20163.finish();
        // GEMREQ-end A_19439, A_20373, A_20365

        A_20163.start(
            R"(7. Die E-Rezept-VAU MUSS aus dem "sub"-Feld-Wert mittels des CMAC-Schlüssels den 128 Bit langen CMAC-Wert
                          berechnen und hexadezimal kodieren (32 Byte lang). Dies sei das Prenutzerpseudonym (PNP).)");
        auto sub = innerServerRequest->getAccessToken().stringForClaim(JWT::subClaim);
        Expect(sub.has_value(), "Missing Sub field in JWT claim");

        if (!Configuration::instance().getOptionalBoolValue(ConfigurationKey::DEBUG_DISABLE_DOS_CHECK, false))
        {
            handleDosCheck(outerSession, sub, innerServerRequest->getAccessToken().intForClaim(JWT::expClaim).value());
        }

        auto&& PnPVerifier = outerSession.serviceContext.getPreUserPseudonymManager();
        CmacSignature preUserPseudonym{getPreUserPseudonym(PnPVerifier, upParam, *sub, outerSession)};
        A_20163.finish();
        A_20163.start(
            "10. Die E-Rezept-VAU MUSS c' und das PNP an die Webschnittstelle als Antwort übergeben.");
        outerSession.response.setHeader(std::string{PreUserPseudonymManager::PNPHeader},
                                   preUserPseudonym.hex());
        TVLOG(2) << "adding PNP: " << preUserPseudonym.hex();
        A_20163.finish();

        ErpExpect(matchingHandler.pathParameters.size() == matchingHandler.handlerContext->pathParameterNames.size(),
                  HttpStatus::BadRequest, "Parameter mismatch.");
        innerSession.request.setPathParameters(matchingHandler.handlerContext->pathParameterNames,
                                                matchingHandler.pathParameters);
        innerSession.request.setFragment(std::move(matchingHandler.fragment));
        innerSession.request.setQueryParameters(std::move(matchingHandler.queryParameters));

        // GEMREQ-start role-check
        if (checkProfessionOID(innerServerRequest,
                               matchingHandler.handlerContext->handler.get(), innerSession.response, outerSession.accessLog))
        {
            matchingHandler.handlerContext->handler->preHandleRequestHook(innerSession);

            std::exception_ptr currExc;
            bool shouldCreateAuditEvent = false;
            try
            {
                // Run the secondary handler
                A_20163.start("5 - process inner request");
                matchingHandler.handlerContext->handler->handleRequest(innerSession);
                A_20163.finish();
                // GEMREQ-end role-check
                transferResponseHeadersFromInnerSession(innerSession, outerSession,
                                                        innerSession.accessLog.getPrescriptionId());
                shouldCreateAuditEvent = innerSession.auditDataCollector().shouldCreateAuditEventOnSuccess();
            }
            catch(const ErpException& exc)
            {
                transferResponseHeadersFromInnerSession(innerSession, outerSession,
                                                        innerSession.accessLog.getPrescriptionId());
                // check if to write an audit event for error case:
                shouldCreateAuditEvent = innerSession.auditDataCollector().shouldCreateAuditEventOnError(exc.status());
                if(!shouldCreateAuditEvent)
                    throw;
                currExc = std::current_exception();
            }

            if (shouldCreateAuditEvent)
            {
                storeAuditData(innerSession, innerServerRequest->getAccessToken());
            }

            A_18936.start("commit transaction");
            auto transaction = innerSession.releaseDatabase();
            if (transaction)
            {
                transaction->commitTransaction();
                transaction.reset();
            }
            A_18936.finish();

            // rethrow for error case to assure that error response is sent;
            if (currExc)
                std::rethrow_exception(currExc);
        }
    }
    // GEMREQ-start A_19439#catchError
    catch(...)
    {
        processException(std::current_exception(), innerServerRequest, innerServerResponse, outerSession);
    }
    // GEMREQ-end A_19439#catchError
    makeResponse(innerServerResponse, innerOperation, innerServerRequest.get(), *innerTeeRequest, outerSession);
}


void VauRequestHandler::makeResponse(ServerResponse& innerServerResponse, const Operation& innerOperation,
                                     const ServerRequest* innerServerRequest, const InnerTeeRequest& innerTeeRequest,
                                     PcSessionContext& outerSession)
{
    try
    {
        handleKeepAlive(outerSession, innerServerRequest, innerServerResponse);

        OuterTeeResponse outerTeeResponse =
            ErpTeeProtocol::encrypt(innerServerResponse, innerTeeRequest.aesKey(), innerTeeRequest.requestId());

        outerTeeResponse.convert(outerSession.response);

        // In case of certain jwt related exceptions (those who fail early), Operation remains UNKNOWN.
        // The Inner-* header for the java-proxy
        outerSession.response.setHeader(Header::InnerResponseCode,
                                   std::to_string(magic_enum::enum_integer(innerServerResponse.getHeader().status())));

        if (! outerSession.response.getHeader(Header::InnerRequestOperation))
        {
            outerSession.response.setHeader(Header::InnerRequestOperation, std::string(toString(innerOperation)));
        }
        outerSession.response.setHeader(
            Header::BackendDurationMs,
            std::to_string(
                std::chrono::duration_cast<std::chrono::milliseconds>(outerSession.backendDuration).count()));

        if (innerServerRequest)
        {
            const auto clientId = innerServerRequest->getAccessToken().stringForClaim(JWT::clientIdClaim);
            if (clientId.has_value()) {
                outerSession.response.setHeader(Header::InnerRequestClientId, clientId.value());
            }
            const auto professionOIDClaim =
                innerServerRequest->getAccessToken().stringForClaim(JWT::professionOIDClaim).value_or("");
            outerSession.response.setHeader(Header::InnerRequestRole,
                                       std::string(profession_oid::toInnerRequestRole(professionOIDClaim)));

            A_22698.start("#4,#5 Create LEI.Telematik-ID pseudonym for and set value in the outer header field.");
            A_22975.start("Use feature only if enabled in configuration");
            if (Configuration::instance().getBoolValue(ConfigurationKey::REPORT_LEIPS_KEY_ENABLE) && professionOIDClaim != profession_oid::oid_versicherter)
            {
                const auto telematikId = innerServerRequest->getAccessToken().stringForClaim(JWT::idNumberClaim);
                if (telematikId.has_value())
                {
                    outerSession.response.setHeader(Header::InnerRequestLeips, PseudonameKeyRefreshJob::hkdf(telematikId.value()));
                }
            }
            A_22975.finish();
            A_22698.finish();
        }
    }
    catch (const std::exception& e)
    {
        // outer response with error:
        ResponseBuilder(outerSession.response).status(HttpStatus::InternalServerError).clearBody().keepAlive(false);
        outerSession.accessLog.locationFromException(e);
        const auto& typeinfo = typeid(e).name();
        outerSession.accessLog.error(
            std::string("preparing, encrypting or sending of response failed: std::exception (") + typeinfo + ")");
    }
    catch (...)
    {
        // outer response with error:
        ResponseBuilder(outerSession.response).status(HttpStatus::InternalServerError).clearBody().keepAlive(false);
        outerSession.accessLog.error("preparing, encrypting or sending of response failed");
    }

    outerSession.accessLog.updateFromInnerResponse(innerServerResponse);
    outerSession.accessLog.updateFromOuterResponse(outerSession.response);
}


Operation VauRequestHandler::getOperation (void) const
{
    return Operation::POST_VAU_up;
}


// GEMREQ-start checkProfessionOID
bool VauRequestHandler::checkProfessionOID(
    const std::unique_ptr<ServerRequest>& innerRequest,
    const RequestHandlerInterface* handler,
    ServerResponse& response,
    AccessLog& log)
{
    // GEMREQ-start A_19390
    // get professionOID from JWT claim
    A_19390.start("Determine professionOID from ACCESS_TOKEN");
    // we don't require the professionOID claim here, because it is not needed for all endpoints.
    const auto professionOIDClaim =
        innerRequest->getAccessToken().stringForClaim(JWT::professionOIDClaim).value_or("");
    A_19390.finish();
    auto optionalPathIdParameter = innerRequest->getPathParameter("id");

    A_19018.start("Check if professionOID claim is allowed for this endpoint");
    A_19022.start("Check if professionOID claim is allowed for this endpoint");
    A_19026.start("Check if professionOID claim is allowed for this endpoint");
    A_19113_01.start("Check if professionOID claim is allowed for this endpoint");
    A_19166_01.start("Check if professionOID claim is allowed for this endpoint WF-160/169/200/209");
    A_25993.start("Check if professionOID claim is allowed for this endpoint WF-162");
    A_19170_02.start("Check if professionOID claim is allowed for this endpoint");
    A_19230_01.start("Check if professionOID claim is allowed for this endpoint");
    A_19390.start("Check if professionOID claim is allowed for this endpoint");
    A_19395.start("Check if professionOID claim is allowed for this endpoint");
    A_19405_02.start("Check if professionOID claim is allowed for this endpoint");
    A_19446_02.start("Check if professionOID claim is allowed for this endpoint");
    A_22362_01.start("Check if professionOID claim is allowed for this endpoint");
    A_24279.start("Check if professionOID claim is allowed for this endpoint");
    if (! handler->allowedForProfessionOID(professionOIDClaim, optionalPathIdParameter))
    {
        TVLOG(1) << "endpoint is forbidden for professionOID: " << professionOIDClaim;
        fillErrorResponse(response, HttpStatus::Forbidden, innerRequest,
                          "endpoint is forbidden for professionOID " + professionOIDClaim, {}/*diagnostics*/);
        A_19018.finish();
        A_19022.finish();
        A_19026.finish();
        A_19113_01.finish();
        A_19166_01.finish();
        A_25993.finish();
        A_19170_02.finish();
        A_19230_01.finish();
        A_19390.finish();
        A_19395.finish();
        A_19405_02.finish();
        A_19446_02.finish();
        A_22362_01.finish();
        A_24279.finish();
        log.location(fileAndLine);
        log.error("endpoint is forbidden for professionOID");
        return false;
    }
    // GEMREQ-end A_19390
    return true;
}
// GEMREQ-end checkProfessionOID

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void VauRequestHandler::processException(const std::exception_ptr& exceptionPtr,
                                         const std::unique_ptr<ServerRequest>& innerRequest,
                                         ServerResponse& innerResponse, PcSessionContext& outerSession)
{
    try
    {
        // This is a small trick that helps to avoid having a large number of catches in the calling method.
        if (exceptionPtr)
            {std::rethrow_exception(exceptionPtr);}
    }
    catch (const ErpException &e)
    {
        runErpExceptionHandler(e, innerRequest, innerResponse, outerSession);
    }
    catch (const ErpServiceException& e)
    {
        erpServiceExceptionHandler(e, innerRequest, innerResponse, outerSession);
    }
    // GEMREQ-start A_19439#catchJwtError, A_20373#catchExpiredException
    catch (const JwtExpiredException& exception)
    {
        runJwtExceptionHandler(
            exception, innerRequest, innerResponse, outerSession, "JWT expired",
            HttpStatus::Unauthorized, Header::WWWAuthenticate, std::string{wwwAuthenticateErrorInvalidToken()});
    }
    // GEMREQ-end A_20373#catchExpiredException
    catch (const JwtRequiredClaimException& exception)
    {
        A_20369_01.start("Handle missing required claims.");
        runJwtExceptionHandler(
            exception, innerRequest, innerResponse, outerSession, "JWT misses required claims",
            HttpStatus::Unauthorized, Header::WWWAuthenticate, std::string{wwwAuthenticateErrorInvalidToken()});
        A_20369_01.finish();
    }
    catch (const JwtInvalidSignatureException& exception)
    {
        A_19131.start("Handle invalid signature");
        runJwtExceptionHandler(
            exception, innerRequest, innerResponse, outerSession, "JWT has invalid signature",
            HttpStatus::Unauthorized, Header::WWWAuthenticate, std::string{wwwAuthenticateErrorInvalidToken()});
        A_19131.finish();
        outerSession.accessLog.message(exception.what());
    }
    catch (const JwtInvalidRfcFormatException& exception)
    {
        runJwtInvalidRfcFormatExceptionHandler(
            exception, innerRequest, innerResponse, outerSession);
    }
    catch (const JwtInvalidFormatException& exception)
    {
        runJwtExceptionHandler(
            exception, innerRequest, innerResponse, outerSession, "JWT has invalid format",
            HttpStatus::Unauthorized, Header::WWWAuthenticate, std::string{wwwAuthenticateErrorInvalidToken()});
    }
    catch (const JwtInvalidAudClaimException& exception)
    {
        A_21520.start("Handle invalid aud claim");
        runJwtExceptionHandler(
            exception, innerRequest, innerResponse, outerSession, "JWT has invalid aud claim",
            HttpStatus::Unauthorized, Header::WWWAuthenticate, std::string{wwwAuthenticateErrorInvalidToken()});
        A_21520.finish();
    }
    // GEMREQ-end A_19439#catchJwtError
    catch (const TslError& e)
    {
        ResponseBuilder(innerResponse)
            .status(e.getHttpStatus())
            .clearBody()
            .keepAlive(false);
        outerSession.accessLog.locationFromException(e);
    }
    catch (const pqxx::internal_error& e)// directly derived from std::logic_error
    {
        runPqxxExceptionHandler(e, innerRequest, innerResponse, outerSession.accessLog);
    }
    catch (const pqxx::usage_error& e)// directly derived from std::logic_error
    {
        runPqxxExceptionHandler(e, innerRequest, innerResponse, outerSession.accessLog);
    }
    catch (const pqxx::failure& e)// base class for all pqxx runtime errors, derived from std::runtime_error
    {
        runPqxxExceptionHandler(e, innerRequest, innerResponse, outerSession.accessLog);
    }
    catch (const std::exception &e)
    {
        const auto typeinfo = util::demangle(typeid(e).name());
        TVLOG(1) << "caught std::exception (" << typeinfo << ") " << e.what();
        ResponseBuilder(innerResponse)
            .status(HttpStatus::InternalServerError)
            .clearBody()
            .keepAlive(false);
        outerSession.accessLog.locationFromException(e);
        outerSession.accessLog.error(std::string("std::exception (") + typeinfo + ")");
    }
    catch (const boost::exception &e)
    {
        TVLOG(1) << "caught boost::exception " << boost::diagnostic_information(e);
        ResponseBuilder(innerResponse)
            .status(HttpStatus::InternalServerError)
            .clearBody()
            .keepAlive(false);
        outerSession.accessLog.locationFromException(e);
        outerSession.accessLog.error("boost::exception");
    }
    catch (...)
    {
        // Every other exception is converted into an internal server error, which, according to table 1 in A_19514,
        // - is valid for all operations
        // - does not allow for additional information.
        TVLOG(1) << "caught throwable that is not derived from std::exception or boost::exception";
        ResponseBuilder(innerResponse)
            .status(HttpStatus::InternalServerError)
            .clearBody()
            .keepAlive(false);
        outerSession.accessLog.error("unknown");
    }
}


void VauRequestHandler::handleKeepAlive (
    PcSessionContext& session,
    const ServerRequest* innerRequest,
    const ServerResponse& innerResponse)
{
    // The response will negate the default support of HTTPS/1.1 for keep-alive if
    // - either the request did not want it
    // - or the inner response has an error status code
    if ( !innerRequest || ! innerRequest->header().keepAlive()
        || toNumericalValue(innerResponse.getHeader().status()) >= 400)
    {
        // Setting kee-alive to false will add header field "Connection: close".
        session.response.setKeepAlive(false);

        if (innerRequest && innerRequest->header().keepAlive())
            TVLOG(1) << "disabled keep-alive because of an error in the inner response";
        else
            TVLOG(1) << "disabled keep-alive because it was not requested";
    }
    else
    {
        // As this is the default for HTTPS/1.1 setting keep-alive to true will do nothing.
        session.response.setKeepAlive(true);
    }
}

CmacSignature VauRequestHandler::getPreUserPseudonym(
    PreUserPseudonymManager& PnPVerifier,
    const std::string& upParam,
    const std::string& sub,
    PcSessionContext& session)
{
    if (upParam == "0")
    {
        return PnPVerifier.sign(sub);
    }
    else
    {
        auto pup = PreUserPseudonym::fromUserPseudonym(upParam);
        auto [verified, preUserPseudonym] = PnPVerifier.verifyAndReSign(pup.getSignature(), sub);
        if (! verified)
        {
            TVLOG(1) << "PNP verification failed";
            session.accessLog.location(fileAndLine);
            session.accessLog.error("PNP verification failed");
        }
        return preUserPseudonym;
    }
}


void VauRequestHandler::handleDosCheck(PcSessionContext& session, const std::optional<std::string>& sub, int64_t exp)
{
    A_19992.start("Run a redis black list for the tokens sub claim.");
    Expect(exp > 0, "Missing exp field in JWT claim");
    auto exp_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::time_point<std::chrono::system_clock>() + std::chrono::seconds(exp));
    bool isWhitelisted = session.serviceContext.getDosHandler().updateCallsCounter(sub.value(), exp_ms);
    VauExpect(isWhitelisted, HttpStatus::TooManyRequests, VauErrorCode::brute_force, "Token with given sub used too often within certain timespan.");
    A_19992.finish();
}


// GEMREQ-start A_20365#getIdpPublicKey
shared_EVP_PKEY VauRequestHandler::getIdpPublicKey (const PcServiceContext& serviceContext)
{
    A_20365_01.start("Get access to the IDP pubkey.");
    return serviceContext.idp.getCertificate().getPublicKey();
    A_20365_01.finish();
}
// GEMREQ-end A_20365#getIdpPublicKey

void VauRequestHandler::transferResponseHeadersFromInnerSession(
    const PcSessionContext& innerSession, PcSessionContext& outerSession,
    const std::optional<model::PrescriptionId>& prescriptionId)
{
    for (const auto& outerHeaderField : innerSession.getOuterResponseHeaderFields())
    {
        outerSession.response.setHeader(outerHeaderField.first, outerHeaderField.second);
    }
    if (prescriptionId)
    {
        A_23090_02.start("\"vnr\": $vorgangsnummer: Task-ID im Fachdienst, Datentyp String");
        outerSession.response.setHeader(Header::PrescriptionId, prescriptionId->toString());
        A_23090_02.finish();
        outerSession.response.setHeader(Header::InnerRequestFlowtype,
                                        std::to_string(magic_enum::enum_integer(prescriptionId->type())));
    }
}
