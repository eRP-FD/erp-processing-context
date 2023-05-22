/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/client/request/ClientRequest.hxx"

#include "erp/util/Expect.hxx"


ClientRequest::ClientRequest(Header aHeader, std::string aBody)
: header(std::move(aHeader))
{
    Expect3(header.method() != HttpMethod::UNKNOWN, "http method must be set for client request", std::logic_error);
    setBody(std::move(aBody));
}


void ClientRequest::setHeader(const std::string& key, const std::string& value)
{
    header.addHeaderField(key, value);
}


const Header& ClientRequest::getHeader (void) const
{
    return header;
}


void ClientRequest::setBody(std::string aBody)
{
    body = std::move(aBody);
    header.setContentLength(body.length());
}


const std::string& ClientRequest::getBody(void) const
{
    return body;
}
