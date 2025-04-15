/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_SHARED_SERVER_SESSION_REQUESTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_SHARED_SERVER_SESSION_REQUESTHANDLER_HXX

#include "shared/server/handler/RequestHandlerInterface.hxx"
#include "shared/util/Configuration.hxx"

class PcServiceContext;

class UnconstrainedRequestHandler : public RequestHandlerInterface
{
public:
    [[nodiscard]] bool allowedForProfessionOID(std::string_view, const std::optional<std::string>&) const override
    {
        return true;
    }
};

class RequestHandlerBasicAuthentication : public UnconstrainedRequestHandler
{
public:
    using Type = RequestHandlerBasicAuthentication;

    void handleBasicAuthentication(const BaseSessionContext& session, ConfigurationKey credentialsKey) const;
};

#endif
