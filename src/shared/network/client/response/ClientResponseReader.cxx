/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/response/ClientResponseReader.hxx"

#include "shared/beast/BoostBeastHeader.hxx"
#include "shared/beast/BoostBeastStringReader.hxx"
#include "shared/ErpConstants.hxx"


ClientResponseReader::ClientResponseReader(std::optional<std::uint64_t> bodyLimit)
    : mBuffer()
    , mParser()
    , mBodyLimit(bodyLimit)
{
    if (mBodyLimit.has_value())
    {
        mParser.body_limit(*mBodyLimit);
    }
    else
    {
        mParser.body_limit(boost::none);
    }
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
    auto [header, body] = BoostBeastStringReader::parseResponse(s, mBodyLimit);
    return ClientResponse{header,std::string(body)};
}
