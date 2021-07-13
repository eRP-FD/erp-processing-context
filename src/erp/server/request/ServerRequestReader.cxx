#include "erp/server/request/ServerRequestReader.hxx"

#include "erp/beast/BoostBeastHeader.hxx"
#include "erp/util/TLog.hxx"

#include "erp/common/BoostBeastMethod.hxx"
#include "erp/server/ErrorHandler.hxx"


ServerRequestReader::ServerRequestReader (SslStream& stream)
    : mStream(stream),
      mBuffer(),
      mParser(),
      mIsStreamClosed(false)
{
}


Header ServerRequestReader::readHeader (void)
{
    mParser.body_limit(ErpConstants::MaxBodySize);
    boost::beast::error_code ec;
    read_header(mStream, mBuffer, mParser, ec);
    if (ec == boost::beast::http::error::end_of_stream ||
        ec == boost::beast::http::error::partial_message ||
        ec == boost::asio::ssl::error::stream_truncated)
    {
        TVLOG(1) << "TcpRequestReader : socket closed";
        closeConnection(true);
    }
    else if (ec)
    {
        ErrorHandler(ec).reportServerError("ServerRequestReader::readHeader");
        closeConnection(false);
    }

    return BoostBeastHeader::fromBeastRequestParser(mParser);
}


ServerRequest ServerRequestReader::read (void)
{
    ServerRequest request (readHeader());

    // Read the whole body synchronously.
    boost::beast::error_code ec;
    boost::beast::http::read(mStream, mBuffer, mParser, ec);
    ErrorHandler(ec).throwOnServerError("ServerRequestReader::read");

    // Transfer ownership of the body string to the request object.
    request.setBody(std::move(mParser.get().body()));

    return request;
}


void ServerRequestReader::markStreamAsClosed (void)
{
    mIsStreamClosed = true;
}


void ServerRequestReader::closeConnection (const bool expectError)
{
    markStreamAsClosed();

    try
    {
        mStream.shutdown();
    }
    catch(const boost::system::system_error& e)
    {
        if (expectError)
        {
            // We are closing the connection hard. This is supposed the reaction to an earlier error where
            // either client or (this) server has made an error. Therefore some data may not have been read from
            // the stream and proper shutdown protocol may impossible to perform.
            //
            // As a result we expect an exception being thrown. And ignore it.
            LOG(ERROR) << e.what();
        }
        else
        {
            // Expected a clean shutdown. Forwarding the exception.
            throw;
        }
    }
}


bool ServerRequestReader::isStreamClosed (void) const
{
    return mIsStreamClosed;
}
