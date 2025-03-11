/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_EXPORTER_HEALTHHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_EXPORTER_HEALTHHANDLER_HXX

#include "exporter/server/SessionContext.hxx"
#include "shared/server/handler/RequestHandlerInterface.hxx"

class HealthHandler : public RequestHandlerInterface
{
public:
    [[nodiscard]] bool allowedForProfessionOID(std::string_view, const std::optional<std::string>&) const override
    {
        return true;
    }

    void handleRequest(BaseSessionContext& session) override
    {
        handleRequest(dynamic_cast<SessionContext&>(session));
    }

    Operation getOperation(void) const override
    {
        return Operation::GET_Health;
    }

private:
    void handleRequest(SessionContext& session);
};

#endif
