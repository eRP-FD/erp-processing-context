/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_SESSION_REQUESTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_SESSION_REQUESTHANDLER_HXX

#include "erp/server/context/SessionContext.hxx"
#include "shared/server/PartialRequestHandler.hxx"
#include "shared/server/handler/RequestHandlerInterface.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/server/RequestHandler.hxx"

class PcServiceContext;

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
