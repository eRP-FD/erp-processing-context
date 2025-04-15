/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/response/ClientResponse.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/util/Expect.hxx"

ClientResponse::ClientResponse()
{
    mHeader.setContentLength(mBody.size());
}
ClientResponse::ClientResponse(Header header, std::string body)
    : mHeader(std::move(header)), mBody(std::move(body))
{
    Expect3(mHeader.method() == HttpMethod::UNKNOWN, "http method must not be set for client response", std::logic_error);
    // For example when Transfer-Encoding: chunked is used, the Content-Length is omitted
    Expect(! mHeader.hasHeader(Header::ContentLength) || mHeader.contentLength() == mBody.size(),
        std::string("header.contentLength (")
            .append(std::to_string(mHeader.contentLength()))
            .append(") does not match body.size: ")
            .append(std::to_string(mBody.size())));
}
const Header& ClientResponse::getHeader() const
{
    return mHeader;
}
const std::string& ClientResponse::getBody() const
{
    return mBody;
}
