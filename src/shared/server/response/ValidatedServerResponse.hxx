/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_RESPONSE_VALIDATEDSERVERRESPONSE_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_RESPONSE_VALIDATEDSERVERRESPONSE_HXX

#include "shared/server/response/ServerResponse.hxx"


// Server response wrapper which validates the header invariants implicitly in constructor:

class ValidatedServerResponse
{
public:
    explicit ValidatedServerResponse (ServerResponse&& serverResponse);

    const Header& getHeader() const;

    const std::string& getBody() const;

private:
    const ServerResponse mServerResponse;
};


#endif
