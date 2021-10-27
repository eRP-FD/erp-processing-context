/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_VAUREQUESTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_VAUREQUESTHANDLER_HXX

#include "erp/crypto/OpenSslHelper.hxx"

#include "erp/crypto/Jwt.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/server/handler/RequestHandlerManager.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/pc/PcServiceContext.hxx"


class AccessLog;
class InnerTeeRequest;
class ServerRequest;


/**
 * Name chosen after the endpoint that is handled (/VAU/).
 */
class VauRequestHandler : public UnconstrainedRequestHandler<PcServiceContext>
{
public:
    static constexpr std::string_view wwwAuthenticateErrorTiRequest()
    {
        return "Bearer realm='prescriptionserver.telematik' scope=openid profile prescriptionservice.lei";
    }
    static constexpr std::string_view wwwAuthenticateErrorInternetRequest()
    {
        return "Bearer realm='prescriptionserver.telematik' scope=openid profile prescriptionservice.vers";
    }
    static constexpr std::string_view wwwAuthenticateErrorInvalidToken()
    {
        return "Bearer realm='prescriptionserver.telematik', error='invalACCESS_TOKEN'";
    }

    explicit VauRequestHandler(RequestHandlerManager<PcServiceContext>&& handlers);

    void handleRequest(PcSessionContext& session) override;
    Operation getOperation (void) const override;

private:
    const RequestHandlerManager<PcServiceContext> mRequestHandlers;

    static std::unique_ptr<InnerTeeRequest> decryptRequest(PcSessionContext& session);

    static bool checkProfessionOID(
        const std::unique_ptr<ServerRequest>& innerRequest,
        const RequestHandlerInterface* handler,
        ServerResponse& response,
        AccessLog& log);
    static void processException(
        const std::exception_ptr& exception,
        const std::unique_ptr<ServerRequest>& innerRequest,
        ServerResponse& innerResponse,
        PcSessionContext& outerSession);
    static void handleKeepAlive (
        PcSessionContext& session,
        const ServerRequest* innerRequest,
        const ServerResponse& innerResponse);
    void handleDosCheck(PcSessionContext& session, const std::optional<std::string>& sub, int64_t exp);
    static CmacSignature getPreUserPseudonym(
        PreUserPseudonymManager& PnPVerifier,
        const std::string& upParam,
        const std::string& sub,
        PcSessionContext& session);
    static shared_EVP_PKEY getIdpPublicKey (const PcServiceContext& serviceContext);
};


#endif
