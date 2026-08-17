/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_EXPORTER_PARTIALREQUESTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_EXPORTER_PARTIALREQUESTHANDLER_HXX

#include "shared/server/BaseServerSession.hxx"

class MatchingHandler;

class PartialRequestHandler : public AbstractRequestHandler
{
public:
    const RequestHandlerManager& mRequestHandlers;

    PartialRequestHandler(const RequestHandlerManager& requestHandlers);

    HandlerResult handleRequest(ServerRequest& request, AccessLog& accessLog) override;
};


#endif// ERP_PROCESSING_CONTEXT_SERVER_EXPORTER_PARTIALREQUESTHANDLER_HXX
