/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/request/ValidatedClientRequest.hxx"


ValidatedClientRequest::ValidatedClientRequest(const ClientRequest& aClientRequest)
: clientRequest(aClientRequest)
{
    clientRequest.getHeader().checkInvariants();
}


const Header& ValidatedClientRequest::getHeader() const
{
    return clientRequest.getHeader();
}


const std::string& ValidatedClientRequest::getBody() const
{
    return clientRequest.getBody();
}
