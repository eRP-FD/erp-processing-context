/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/server/RequestHandler.hxx"
#include "shared/util/Expect.hxx"


void RequestHandlerBasicAuthentication::handleBasicAuthentication(const BaseSessionContext& session,
                                                                  ConfigurationKey credentialsKey) const
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
