/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_REQUEST_VALIDATEDCLIENTREQUEST_H
#define ERP_PROCESSING_CONTEXT_CLIENT_REQUEST_VALIDATEDCLIENTREQUEST_H

#include "erp/client/request/ClientRequest.hxx"


// Client request wrapper which validates the header invariants implicitly in constructor:

class ValidatedClientRequest
{
public:
    explicit ValidatedClientRequest(const ClientRequest& aClientRequest);

    const Header& getHeader() const;

    const std::string& getBody() const;

private:
    const ClientRequest& clientRequest;
};


#endif
