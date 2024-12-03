/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/response/ClientResponseReader.hxx"

#include "shared/beast/BoostBeastHeader.hxx"
#include "shared/beast/BoostBeastStringReader.hxx"
#include "shared/ErpConstants.hxx"


ClientResponseReader::ClientResponseReader (void)
    : mBuffer(),
      mParser(),
      mIsStreamClosed(false)
{
    mParser.body_limit(ErpConstants::MaxResponseBodySize);
}


Header ClientResponseReader::convertHeader (void)
{
    return BoostBeastHeader::fromBeastResponseParser(mParser);
}


void ClientResponseReader::markStreamAsClosed (void)
{
    mIsStreamClosed = true;
}


bool ClientResponseReader::isStreamClosed (void) const
{
    return mIsStreamClosed;
}


ClientResponse ClientResponseReader::read (const std::string& s)
{
    auto [header,body] = BoostBeastStringReader::parseResponse(s);
    return ClientResponse{header,std::string(body)};
}
