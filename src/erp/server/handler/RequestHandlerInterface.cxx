/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/request/ServerRequest.hxx"


void RequestHandlerBasicAuthentication::handleBasicAuthentication(
    const SessionContext& session, ConfigurationKey credentialsKey) const
{
    SafeString secret(Configuration::instance().getSafeStringValue(credentialsKey));
    ErpExpect(session.request.header().hasHeader(Header::Authorization), HttpStatus::Unauthorized,
              "Authorization required.");
    auto parts = String::split(session.request.header().header(Header::Authorization).value_or(""), " ");
    if (parts.size() == 2)
    {
        SafeString authSecret(std::move(parts[1]));
        ErpExpect(String::toLower(parts[0]) == "basic" && authSecret == secret, HttpStatus::Unauthorized,
                  "Unauthorized");
    }
    else
    {
        ErpExpect(false, HttpStatus::Unauthorized, "Unauthorized (unsupported authorization content.)");
    }
}
