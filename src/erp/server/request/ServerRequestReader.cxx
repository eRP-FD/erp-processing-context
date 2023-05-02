/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

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
    ErrorHandler(ec).throwOnServerError("ServerRequestReader::read", {__FILE__, __LINE__});

    // Transfer ownership of the body string to the request object.
    request.setBody(std::move(mParser.get().body()));

    return request;
}

void ServerRequestReader::readAsynchronously (RequestConsumer&& requestConsumer)
{
    mParser.body_limit(ErpConstants::MaxBodySize);

    mStream.expiresAfter(std::chrono::seconds(ErpConstants::SocketTimeoutSeconds));

    // Read the whole body synchronously.
    boost::beast::http::async_read(
        mStream,
        mBuffer,
        mParser,
        [this, requestConsumer=std::move(requestConsumer), logContext = erp::server::Worker::tlogContext]
        (const boost::beast::error_code& ec, const size_t count)
        { on_async_read(requestConsumer, logContext, ec, count);});
}

void ServerRequestReader::on_async_read(const ServerRequestReader::RequestConsumer& requestConsumer,
                                        const std::optional<std::string>& logContext,
                                        const boost::beast::error_code& ec, const size_t count)
{
    ScopedLogContext logScope{logContext};
    if (count == 0)
    {
        TVLOG(0) << "closing stream.";
        closeConnection(true);
        if (ec != boost::beast::http::error::end_of_stream &&
            ec != boost::asio::ssl::error::stream_truncated)
        {
            TLOG(WARNING) << "error on stream while waiting for next request: " << ec.message();
        }
        requestConsumer({}, {});
        return;
    }
    if (ec)
    {
        TLOG(WARNING) << "after reading " << count << " bytes: " << ec.message();
    }
    else
    {
        TVLOG(2) << "request bytes: " << count;
    }

    try
    {
        ErrorHandler(ec).throwOnServerError("ServerRequestReader::readAsynchronously", fileAndLine);

        // Parse the request.
        ServerRequest request (BoostBeastHeader::fromBeastRequestParser(mParser));

        // Transfer ownership of the body string to the request object.
        request.setBody(std::move(mParser.get().body()));

        // Note that the requestConsumer callback is expected to be (effectively) `noexcept` and therefore does
        // not throw any exceptions.
        requestConsumer(std::move(request), {});
    }
    catch(...)
    {
        requestConsumer(std::nullopt, std::current_exception());
    }
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
        if (e.code() == boost::asio::error::not_connected)
        {
            TVLOG(3) << "stream already closed";
        }
        else if (expectError)
        {
            // We are closing the connection hard. This is supposed the reaction to an earlier error where
            // either client or (this) server has made an error. Therefore some data may not have been read from
            // the stream and proper shutdown protocol may impossible to perform.
            //
            // As a result we expect an exception being thrown. And ignore it.
            TLOG(ERROR) << e.what();
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
