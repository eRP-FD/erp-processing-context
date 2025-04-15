/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_REQUEST_CLIENTREQUEST_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_REQUEST_CLIENTREQUEST_HXX

#include "shared/network/message/Header.hxx"

#include <string>


class ClientRequest
{
public:
    ClientRequest(Header aHeader, std::string aBody);

    void setHeader(const std::string& key, const std::string& value);
    const Header& getHeader (void) const;

    void setBody(std::string aBody);
    const std::string& getBody(void) const;

private:
    Header header;
    std::string body;

    friend class ErpWorkflowTestBase;
};

#endif
