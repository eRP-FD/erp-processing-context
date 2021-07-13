#include "erp/service/VauRequestHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/crypto/AesGcm.hxx"
#include "erp/crypto/CMAC.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/Device.hxx"
#include "erp/pc/ProfessionOid.hxx"
#include "erp/pc/pre_user_pseudonym/PreUserPseudonym.hxx"
#include "erp/pc/pre_user_pseudonym/PreUserPseudonymManager.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ResponseBuilder.hxx"
#include "erp/server/response/ResponseValidator.hxx"
#include "erp/server/session/ServerSession.hxx"
#include "erp/service/DosHandler.hxx"
#include "erp/tee/ErpTeeProtocol.hxx"
#include "erp/tee/InnerTeeRequest.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/JwtException.hxx"
#include "erp/util/TLog.hxx"

#include <boost/exception/diagnostic_information.hpp>
#include <unordered_set>


namespace
{

std::unordered_set<Operation> auditRelevantOperations = {
    Operation::GET_Task,
    Operation::GET_Task_id,
    Operation::POST_Task_id_activate,
    Operation::POST_Task_id_accept,
    Operation::POST_Task_id_reject,
    Operation::POST_Task_id_close,
    Operation::POST_Task_id_abort,
    Operation::GET_MedicationDispense,
    Operation::GET_MedicationDispense_id
};


void storeAuditData(PcSessionContext& sessionContext, const JWT& accessToken)
{
    A_19391.start("Use name of caller for audit logging");
    A_19392.start("Use id of caller for audit logging");
    sessionContext.auditDataCollector().fillFromAccessToken(accessToken);
    A_19391.finish();
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
        LOG(WARNING) << "Missing Audit relevant data, unable to create Audit data record";
        sessionContext.accessLog.locationFromException(exc);
        sessionContext.accessLog.error("Missing Audit relevant data, unable to create Audit data record");
        throw;
    }
    catch(const std::exception& exc)
    {
        LOG(WARNING) << "Error while storing Audit data";
        TVLOG(1) << "Error reason:  " << exc.what();
        sessionContext.accessLog.locationFromException(exc);
        sessionContext.accessLog.error("Error while storing Audit data");
        throw;
    }
    catch(...)
    {
        LOG(WARNING) << "Unknown error while storing Audit data";
        sessionContext.accessLog.error("Unknown error while storing Audit data");
        throw;
    }
}

JWT getJwtFromAuthorizationHeader(const std::string& authorizationHeaderValue)
{
    ErpExpect(String::starts_with( authorizationHeaderValue , "Bearer "), HttpStatus::BadRequest, "invalid Authorization Bearer header field");
    return JWT(authorizationHeaderValue.substr(7));
}

} // anonymous namespace


namespace exception_handlers
{
void runErpExceptionHandler(const ErpException& exception, ServerResponse& innerResponse, PcSessionContext& outerSession)
{
    using namespace std::string_literals;
    TVLOG(1) << "caught ErpException " << exception.what();
    outerSession.accessLog.locationFromException(exception);
    switch (exception.status())
    {
        case HttpStatus::BadRequest:
            outerSession.accessLog.error("ErpException: "s + exception.what());
            break;
        default:
            outerSession.accessLog.error("ErpException"s);
    }
    ResponseBuilder(innerResponse).status(exception.status()).clearBody().keepAlive(false);
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


void runJwtExceptionHandler(const JwtException& exception, ServerResponse& innerResponse, HttpStatus httpStatus,
                            const std::string& headerField, const std::string& headerValue)
{
    TVLOG(1) << exception.what();
    if (!headerField.empty() && !headerValue.empty())
    {
        innerResponse.setHeader(headerField, headerValue);
    }
    ResponseBuilder(innerResponse).status(httpStatus).clearBody().keepAlive(false);
}


void runJwtInvalidFormatExceptionHandler(const JwtException& exception, ServerResponse& innerResponse, HttpStatus httpStatus,
                                         const std::string& headerField, const std::string& headerValue)
{
    TVLOG(1) << exception.what();
    if (!headerField.empty() && !headerValue.empty())
    {
        innerResponse.setHeader(headerField, headerValue);
    }
    ResponseBuilder(innerResponse).status(httpStatus).clearBody().keepAlive(false);
}


void runJwtInvalidRfcFormatExceptionHandler(const JwtInvalidRfcFormatException& exception, ServerResponse& innerResponse,
                                              const std::optional<std::string>& externalInterface)
{
    TVLOG(1) << exception.what();
    if (externalInterface.has_value() && externalInterface.value() == Header::ExternalInterface_TI)
    {
        A_19130.start("Invalid format from a 'lei' request");
        innerResponse.setHeader(Header::WWWAuthenticate,
                                std::string{VauRequestHandler::wwwAuthenticateErrorTiRequest()});
        ResponseBuilder(innerResponse).status(HttpStatus::Unauthorized).clearBody().keepAlive(false);
        A_19130.finish();
    }
    else if (externalInterface.has_value() && externalInterface.value() == Header::ExternalInterface_INTERNET)
    {
        A_19389.start("Invalid format from a 'vers' request");
        innerResponse.setHeader(Header::WWWAuthenticate,
                                std::string{VauRequestHandler::wwwAuthenticateErrorInternetRequest()});
        ResponseBuilder(innerResponse).status(HttpStatus::Unauthorized).clearBody().keepAlive(false);
        A_19389.finish();
    }
    else
    {
        innerResponse.setHeader(Header::WWWAuthenticate,
                                std::string{VauRequestHandler::wwwAuthenticateErrorInvalidToken()});
        ResponseBuilder(innerResponse).status(HttpStatus::Unauthorized).clearBody().keepAlive(false);
    }
}
}


VauRequestHandler::VauRequestHandler(RequestHandlerManager<PcServiceContext>&& handlers)
    : mRequestHandlers(std::move(handlers))
{
}


void VauRequestHandler::handleRequest(PcSessionContext& session)
{
    // Set up the duration consumer that will output times spend on calls to external services without the need
    // of explicitly passing an object to the downloading code.
    DurationConsumerGuard durationConsumerGuard (
        session.request.header().header(Header::XRequestId).value_or("unknown X-Request-Id"),
        []
        (const std::chrono::system_clock::duration duration, const std::string& description, const std::string& sessionIdentifier)
        {
            JsonLog(LogId::INFO)
                .keyValue("log-type", "timing")
                .keyValue("x-request-id", sessionIdentifier)
                .keyValue("description", description)
                .keyValue("duration-ms", gsl::narrow<size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()));
        });

    // Set X-Request-Id as soon as possible
    session.accessLog.updateFromOuterRequest(session.request);

    auto upParam = session.request.getPathParameter("UP");
    Expect(upParam.has_value(), "Missing Pre-User-Pseudonym in Path.");

    std::unique_ptr<InnerTeeRequest> innerTeeRequest;
    std::unique_ptr<ServerRequest> innerServerRequest;
    ServerResponse innerServerResponse;
    // Remove when all endpoints are implemented (or update missing endpoint return values.)
    innerServerResponse.setStatus(HttpStatus::OK);
    Operation innerOperation = Operation::UNKNOWN;
    std::optional<std::string> externalInterface{""};

    try
    {
        // Decrypt
        innerTeeRequest = decryptRequest(session);
        if (nullptr == innerTeeRequest)
        {
            session.accessLog.error("decryption of inner request failed");
            return;
        }

        innerTeeRequest->parseHeaderAndBody();
        innerServerRequest = std::make_unique<ServerRequest>( innerTeeRequest->releaseHeader() );
        innerServerRequest->setBody(innerTeeRequest->releaseBody());

        ErpExpect(innerServerRequest->header().hasHeader(Header::Authorization),
                  HttpStatus::BadRequest, "Authorization header is missing");
        const JWT vauJwt = innerTeeRequest->releaseAuthenticationToken();
        JWT authorizationJwt = getJwtFromAuthorizationHeader(innerServerRequest->header().header(Header::Authorization).value()) ;
        ErpExpect(authorizationJwt == vauJwt,
                  HttpStatus::BadRequest, "Authorization header is missing");
        innerServerRequest->setAccessToken(std::move(authorizationJwt));

        if (innerServerRequest->header().hasHeader(Header::ExternalInterface))
        {
            externalInterface = innerServerRequest->header().header(Header::ExternalInterface);
        }

        // Create an inner session that contains the unencrypted request and a, yet to be filled, unencrypted response.
        PcSessionContext innerSession(session.serviceContext, *innerServerRequest, innerServerResponse);
        innerSession.accessLog.discard();

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
            ErpFail(HttpStatus::MethodNotAllowed, "no matching handler found.");
        }

        // Determnine inner operation. Required for final response.
        auto* handler = matchingHandler.handlerContext->handler.get();
        innerOperation = (handler != nullptr) ? handler->getOperation() : Operation::UNKNOWN;
        session.accessLog.setInnerRequestOperation(toString(innerOperation));

        A_20163.start("4 - verify JWT");
        A_20365.start("Pass the IDP pubkey to the verification method.");
        innerServerRequest->getAccessToken().verify(getIdpPublicKey(session.serviceContext));
        A_20365.finish();
        A_20163.finish();

        A_20163.start(
            R"(7. Die E-Rezept-VAU MUSS aus dem "sub"-Feld-Wert mittels des CMAC-Schlüssels den 128 Bit langen CMAC-Wert
                          berechnen und hexadezimal kodieren (32 Byte lang). Dies sei das Prenutzerpseudonym (PNP).)");
        auto sub = innerServerRequest->getAccessToken().stringForClaim(JWT::subClaim);
        Expect(sub.has_value(), "Missing Sub field in JWT claim");

        if (!Configuration::instance().getOptionalBoolValue(ConfigurationKey::DEBUG_DISABLE_DOS_CHECK, false))
        {
            handleDosCheck(session, sub, innerServerRequest->getAccessToken().intForClaim(JWT::expClaim).value());
        }

        auto&& PnPVerifier = session.serviceContext.getPreUserPseudonymManager();
        CmacSignature preUserPseudonym{getPreUserPseudonym(PnPVerifier, *upParam, *sub, session)};
        A_20163.finish();
        A_20163.start(
            "10. Die E-Rezept-VAU MUSS c' und das PNP an die Webschnittstelle als Antwort übergeben.");
        session.response.setHeader(std::string{PreUserPseudonymManager::PNPHeader},
                                   preUserPseudonym.hex());
        VLOG(1) << "adding PNP: " << preUserPseudonym.hex();
        A_20163.finish();

        ErpExpect(matchingHandler.pathParameters.size() == matchingHandler.handlerContext->pathParameterNames.size(),
                  HttpStatus::BadRequest, "Parameter mismatch.");
        innerSession.request.setPathParameters(matchingHandler.handlerContext->pathParameterNames,
                                                matchingHandler.pathParameters);
        innerSession.request.setFragment(std::move(matchingHandler.fragment));
        innerSession.request.setQueryParameters(std::move(matchingHandler.queryParameters));

        if (checkProfessionOID(innerServerRequest->getAccessToken(),
                               matchingHandler.handlerContext->handler.get(), innerSession.response, session.accessLog))
        {
            matchingHandler.handlerContext->handler->preHandleRequestHook(innerSession);

            // Run the secondary handler
            A_20163.start("5 - process inner request");
            matchingHandler.handlerContext->handler->handleRequest(innerSession);
            A_20163.finish();

            if(auditRelevantOperations.count(innerOperation))
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
        }
    }
    catch(...)
    {
        processException(std::current_exception(), externalInterface, innerServerResponse, session);
    }

    try
    {
        ResponseValidator::validate(innerServerResponse, innerOperation);
        handleKeepAlive(session, innerServerRequest.get(), innerServerResponse);
        // Encrypt the response.
        OuterTeeResponse outerTeeResponse =
            ErpTeeProtocol::encrypt(innerServerResponse, innerTeeRequest->aesKey(), innerTeeRequest->requestId());

        outerTeeResponse.convert(session.response);

        // In case of certain jwt related exceptions (those who fail early), Operation remains UNKNOWN.
        // The Inner-* header for the java-proxy
        session.response.setHeader(Header::InnerResponseCode,
                                   std::to_string(magic_enum::enum_integer(innerServerResponse.getHeader().status())));

        session.response.setHeader(Header::InnerRequestOperation, std::string(toString(innerOperation)));

        if (innerServerRequest)
        {
            const auto professionOIDClaim =
                innerServerRequest->getAccessToken().stringForClaim(JWT::professionOIDClaim).value_or("");
            session.response.setHeader(Header::InnerRequestRole,
                                       std::string(profession_oid::toInnerRequestRole(professionOIDClaim)));
        }
    }
    catch (const std::exception& e)
    {
        // outer response with error:
        ResponseBuilder(session.response).status(HttpStatus::InternalServerError).clearBody().keepAlive(false);
        session.accessLog.locationFromException(e);
        session.accessLog.error("preparing, encrypting or sending of response failed");
    }
    catch (...)
    {
        // outer response with error:
        ResponseBuilder(session.response).status(HttpStatus::InternalServerError).clearBody().keepAlive(false);
        session.accessLog.error("preparing, encrypting or sending of response failed");
    }

    session.accessLog.updateFromInnerRequest(*innerServerRequest);
    session.accessLog.updateFromInnerResponse(innerServerResponse);
    session.accessLog.updateFromOuterResponse(session.response);
}


Operation VauRequestHandler::getOperation (void) const
{
    return Operation::POST_VAU_up;
}


std::unique_ptr<InnerTeeRequest> VauRequestHandler::decryptRequest(PcSessionContext& session)
{
    try
    {
        return std::make_unique<InnerTeeRequest>(
            ErpTeeProtocol::decrypt(session.request.getBody(), session.serviceContext.getHsmPool()));
    }
    catch (const AesGcmException& e)
    {
        // Can't encrypt/decrypt. Only outer response can be provided.
        ResponseBuilder(session.response).status(HttpStatus::BadRequest).clearBody().keepAlive(false);
        session.accessLog.locationFromException(e);
        session.accessLog.error(std::string("vau decryption failed: AesGcmException ") + e.what());
    }
    catch (const ErpException& e)
    {
        ResponseBuilder(session.response).status(e.status()).clearBody().keepAlive(false);
        session.accessLog.locationFromException(e);
        session.accessLog.error(std::string("vau decryption failed: ErpException ") + e.what());
    }
    catch (const JwtException& e)
    {
        ResponseBuilder(session.response).status(HttpStatus::Unauthorized).clearBody().keepAlive(false);
        session.accessLog.locationFromException(e);
        session.accessLog.error(std::string("vau decryption failed: JwtException ") + e.what());
    }
    catch (const std::exception& e)
    {
        ResponseBuilder(session.response).status(HttpStatus::InternalServerError).clearBody().keepAlive(false);
        session.accessLog.locationFromException(e);
        session.accessLog.error(std::string("vau decryption failed: std::exception"));
    }
    catch (...)
    {
        ResponseBuilder(session.response).status(HttpStatus::InternalServerError).clearBody().keepAlive(false);
        session.accessLog.error(std::string("vau decryption failed: unexpected exception"));
    }
    return nullptr;
}


bool VauRequestHandler::checkProfessionOID(
    const JWT& accessToken,
    const RequestHandlerInterface* handler,
    ServerResponse& response,
    AccessLog& log)
{
    // get professionOID from JWT claim
    A_19390.start("Determine professionOID from ACCESS_TOKEN");
    // we don't require the professionOID claim here, because it is not needed for all endpoints.
    const auto professionOIDClaim =
        accessToken.stringForClaim(JWT::professionOIDClaim).value_or("");
    A_19390.finish();

    A_19018.start("Check if professionOID claim is allowed for this endpoint");
    A_19022.start("Check if professionOID claim is allowed for this endpoint");
    A_19026.start("Check if professionOID claim is allowed for this endpoint");
    A_19113_01.start("Check if professionOID claim is allowed for this endpoint");
    A_19166.start("Check if professionOID claim is allowed for this endpoint");
    A_19166.start("Check if professionOID claim is allowed for this endpoint");
    A_19170_01.start("Check if professionOID claim is allowed for this endpoint");
    A_19230.start("Check if professionOID claim is allowed for this endpoint");
    A_19390.start("Check if professionOID claim is allowed for this endpoint");
    A_19395.start("Check if professionOID claim is allowed for this endpoint");
    A_19405.start("Check if professionOID claim is allowed for this endpoint");
    A_19446_01.start("Check if professionOID claim is allowed for this endpoint");
    if (! handler->allowedForProfessionOID(professionOIDClaim))
    {
        VLOG(1) << "endpoint is forbidden for professionOID: " << professionOIDClaim;
        response.setStatus(HttpStatus::Forbidden);
        A_19018.finish();
        A_19022.finish();
        A_19026.finish();
        A_19113_01.finish();
        A_19166.finish();
        A_19166.finish();
        A_19170_01.finish();
        A_19230.finish();
        A_19390.finish();
        A_19395.finish();
        A_19405.finish();
        A_19446_01.finish();
        log.location(fileAndLine);
        log.error("endpoint is forbidden for professionOID");
        return false;
    }
    return true;
}


void VauRequestHandler::processException(const std::exception_ptr& exception,
                                         const std::optional<std::string>& externalInterface,
                                         ServerResponse& innerResponse, PcSessionContext& outerSession)
{
    try
    {
        // This is a small trick that helps to avoid having a large number of catches in the calling method.
        if (exception)
            std::rethrow_exception(exception);
    }
    catch (const ErpException &e)
    {
        exception_handlers::runErpExceptionHandler(e, innerResponse, outerSession);
    }
    catch (const JwtExpiredException& exception)
    {
        A_19902.start("Handle authentication expired");
        exception_handlers::runJwtExceptionHandler(
            exception, innerResponse, HttpStatus::Unauthorized, Header::WWWAuthenticate,
            R"(Bearer realm='prescriptionserver.telematik', error='invalACCESS_TOKEN'")");
        A_19902.finish();
        outerSession.accessLog.locationFromException(exception);
        outerSession.accessLog.error("JWT expired");
    }
    catch (const JwtRequiredClaimException& exception)
    {
        A_20368.start("Handle missing required claims.");
        exception_handlers::runJwtExceptionHandler(exception, innerResponse, HttpStatus::Unauthorized,
                                                   Header::WWWAuthenticate,
                                                   std::string{wwwAuthenticateErrorInvalidToken()});
        A_20368.finish();
        outerSession.accessLog.locationFromException(exception);
        outerSession.accessLog.error("JWT misses required claims");
    }
    catch (const JwtInvalidSignatureException& exception)
    {
        A_19131.start("Handle invalid signature");
        exception_handlers::runJwtExceptionHandler(exception, innerResponse, HttpStatus::Unauthorized,
                                                  Header::WWWAuthenticate,
                                                  std::string{wwwAuthenticateErrorInvalidToken()});
        A_19131.finish();
        outerSession.accessLog.locationFromException(exception);
        outerSession.accessLog.error("JWT has invalid signature");
    }
    catch (const JwtInvalidRfcFormatException& exception)
    {
        exception_handlers::runJwtInvalidRfcFormatExceptionHandler(exception, innerResponse, externalInterface);
        outerSession.accessLog.error("JWT has invalid RFC format");
    }
    catch (const JwtInvalidFormatException& exception)
    {
        exception_handlers::runJwtInvalidFormatExceptionHandler(exception, innerResponse, HttpStatus::Unauthorized, Header::WWWAuthenticate, std::string{wwwAuthenticateErrorInvalidToken()});
        outerSession.accessLog.locationFromException(exception);
        outerSession.accessLog.error("JWT has invalid format");
    }
    catch (const JwtInvalidAudClaimException& exception)
    {
        A_21520.start("Handle invalid aud claim");
        exception_handlers::runJwtExceptionHandler(exception, innerResponse, HttpStatus::Unauthorized,
                                                   Header::WWWAuthenticate,
                                                   std::string{wwwAuthenticateErrorInvalidToken()});
        A_21520.finish();
        outerSession.accessLog.locationFromException(exception);
        outerSession.accessLog.error("JWT has invalid aud claim");
    }
    catch (const std::exception &e)
    {
        TVLOG(1) << "caught std::exception " << e.what();
        ResponseBuilder(innerResponse)
            .status(HttpStatus::InternalServerError)
            .clearBody()
            .keepAlive(false);
        outerSession.accessLog.locationFromException(e);
        outerSession.accessLog.error(std::string("std::exception"));
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
        TVLOG(1) << "keep-alive is active";
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
        auto [verified, preUserPseudonym] = PnPVerifier.verifyAndResign(pup.getSignature(), sub);
        if (! verified)
        {
            VLOG(1) << "PNP verification failed";
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
    bool isWhitelisted = session.serviceContext.getDosHandler().updateAccessCounter(sub.value(), exp_ms);
    VauExpect(isWhitelisted, HttpStatus::TooManyRequests, VauErrorCode::brute_force, "Token with given sub used too often within certain timespan.");
    A_19992.finish();
}


shared_EVP_PKEY VauRequestHandler::getIdpPublicKey (const PcServiceContext& serviceContext)
{
    A_20365.start("Get access to the IDP pubkey.");
    return serviceContext.idp.getCertificate().getPublicKey();
    A_20365.finish();
}
