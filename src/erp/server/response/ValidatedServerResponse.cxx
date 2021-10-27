/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/server/response/ValidatedServerResponse.hxx"


ValidatedServerResponse::ValidatedServerResponse (ServerResponse&& serverResponse)
    : mServerResponse(std::move(serverResponse))
{
    mServerResponse.getHeader().checkInvariants();
}


const Header& ValidatedServerResponse::getHeader() const
{
    return mServerResponse.getHeader();
}


const std::string& ValidatedServerResponse::getBody() const
{
    return mServerResponse.getBody();
}
