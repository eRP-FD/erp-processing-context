/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_HANDLER_REQUESTHANDLERINTERFACE_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_HANDLER_REQUESTHANDLERINTERFACE_HXX

#include "shared/server/BaseSessionContext.hxx"
#include "shared/service/Operation.hxx"

#include <string_view>

class RequestHandlerInterface
{
public:
    virtual ~RequestHandlerInterface (void) = default;

    virtual void preHandleRequestHook(BaseSessionContext&) {};
    virtual void handleRequest (BaseSessionContext& session) = 0;
    [[nodiscard]] virtual bool allowedForProfessionOID(std::string_view professionOid) const = 0;
    virtual Operation getOperation (void) const = 0;
};


#endif
