/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_SESSION_REQUESTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_SESSION_REQUESTHANDLER_HXX

#include "erp/server/context/SessionContext.hxx"
#include "shared/server/PartialRequestHandler.hxx"
#include "shared/server/handler/RequestHandlerInterface.hxx"
#include "shared/util/Configuration.hxx"

class PcServiceContext;

class UnconstrainedRequestHandler : public RequestHandlerInterface
{
public:
    [[nodiscard]] bool allowedForProfessionOID(std::string_view) const override
    {
        return true;
    }
    void handleRequest(BaseSessionContext& session) override
    {
        handleRequest(dynamic_cast<SessionContext&>(session));
    }
    virtual void handleRequest(SessionContext& session) = 0;
};

class RequestHandlerBasicAuthentication : public UnconstrainedRequestHandler
{
public:
    using Type = RequestHandlerBasicAuthentication;

    void handleBasicAuthentication(const BaseSessionContext& session, ConfigurationKey credentialsKey) const;
};


class RequestHandler : public PartialRequestHandler
{
public:
    RequestHandler(const RequestHandlerManager& requestHandlers, PcServiceContext& serviceContext)
        : PartialRequestHandler(requestHandlers)
        , mServiceContext(serviceContext)
    {
    }

    PcServiceContext& mServiceContext;

    std::tuple<bool, std::optional<RequestHandlerManager::MatchingHandler>, ServerResponse>
    handleRequest(ServerRequest& request, AccessLog& accessLog) override;
};

#endif
