/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_RESPONSE_CLIENTRESPONSE_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_RESPONSE_CLIENTRESPONSE_HXX

#include "shared/network/message/Header.hxx"

#include <string>


class ClientResponse
{
public:
    ClientResponse();
    ClientResponse(Header header, std::string body);

    const Header& getHeader() const;
    const std::string& getBody() const;

private:
    Header mHeader;
    std::string mBody;
};

#endif
