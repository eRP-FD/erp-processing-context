/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "shared/server/handler/RequestHandlerInterface.hxx"

namespace model
{
class Kvnr;
}

class SessionContext;
class PushersRequestHandler : public RequestHandlerInterface
{
public:
    void preHandleRequestHook(BaseSessionContext&) override;
    void handleRequest(BaseSessionContext& session) override;
    [[nodiscard]] bool
    allowedForProfessionOID(std::string_view professionOid,
                            const std::optional<std::string>& optionalPathIdParameter) const override;

protected:
    virtual void handlePushersRequest(SessionContext& session) = 0;
    static void makeResponse(BaseSessionContext& session, HttpStatus status, const std::string& body);
};
