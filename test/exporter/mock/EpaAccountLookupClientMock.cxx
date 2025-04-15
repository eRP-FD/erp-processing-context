/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "EpaAccountLookupClientMock.hxx"


EpaAccountLookupClientMock& EpaAccountLookupClientMock::setResponseStatus(HttpStatus httpStatus)
{
    mHttpStatus = httpStatus;
    return *this;
}
EpaAccountLookupClientMock& EpaAccountLookupClientMock::setResponseBody(std::string_view responseBody)
{
    mResponseBody = responseBody;
    return *this;
}

ClientResponse EpaAccountLookupClientMock::sendConsentDecisionsRequest(const std::string&, const model::Kvnr&,
                                                                       const std::string&, uint16_t)
{
    return ClientResponse(Header{mHttpStatus}, mResponseBody);
}
