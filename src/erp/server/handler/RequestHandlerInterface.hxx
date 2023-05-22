/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_HANDLER_REQUESTHANDLERINTERFACE_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_HANDLER_REQUESTHANDLERINTERFACE_HXX

#include "erp/service/Operation.hxx"
#include "erp/util/Configuration.hxx"

#include <string_view>


class SessionContext;


class RequestHandlerInterface
{
public:
    virtual ~RequestHandlerInterface (void) = default;

    virtual void preHandleRequestHook(SessionContext&) {};
    virtual void handleRequest (SessionContext& session) = 0;
    [[nodiscard]] virtual bool allowedForProfessionOID(std::string_view professionOid) const = 0;
    virtual Operation getOperation (void) const = 0;
};

class UnconstrainedRequestHandler : public RequestHandlerInterface
{
public:
    [[nodiscard]] bool allowedForProfessionOID (std::string_view) const override {return true;}
};

class RequestHandlerBasicAuthentication : public UnconstrainedRequestHandler
{
public:
    using Type = RequestHandlerBasicAuthentication;

    void handleBasicAuthentication(const SessionContext& session,
                                   ConfigurationKey credentialsKey) const;
};


#endif
