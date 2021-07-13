#include "erp/client/response/ClientResponseReader.hxx"

#include "erp/beast/BoostBeastHeader.hxx"
#include "erp/beast/BoostBeastStringReader.hxx"


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
