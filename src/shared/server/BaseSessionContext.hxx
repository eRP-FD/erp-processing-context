/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_BASESESSIONCONTEXT_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_BASESESSIONCONTEXT_HXX

#include "shared/server/AccessLog.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/server/response/ServerResponse.hxx"

class BaseServiceContext;
namespace model
{
class Kvnr;
}

class BaseSessionContext
{
public:
    BaseSessionContext(BaseServiceContext& serviceContext, ServerRequest& request, ServerResponse& response,
                       AccessLog& accessLog)
        : baseServiceContext(serviceContext)
        , request(request)
        , response(response)
        , accessLog(accessLog)
    {
    }

    virtual ~BaseSessionContext() = default;

    model::Kvnr kvnrFromAccessToken() const;

    BaseServiceContext& baseServiceContext;
    ServerRequest& request;
    ServerResponse& response;
    AccessLog& accessLog;
};

#endif// ERP_PROCESSING_CONTEXT_SERVER_BASESESSIONCONTEXT_HXX
