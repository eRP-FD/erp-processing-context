/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_RESPONSE_VALIDATEDSERVERRESPONSE_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_RESPONSE_VALIDATEDSERVERRESPONSE_HXX

#include "erp/server/response/ServerResponse.hxx"


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
