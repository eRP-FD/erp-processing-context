#include "erp/server/session/ServerSession.hxx"

#include "erp/ErpConstants.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/server/ErrorHandler.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/server/response/ServerResponseWriter.hxx"
#include "erp/server/response/ValidatedServerResponse.hxx"
#include "erp/tee/ErpTeeProtocol.hxx"
#include "erp/util/JwtException.hxx"
#include "erp/util/TLog.hxx"

#include <boost/beast/http/write.hpp>
#include <boost/exception/diagnostic_information.hpp>


ServerSession::ServerSession (
    boost::asio::ip::tcp::socket&& socket,
    boost::asio::ssl::context& context,
    std::unique_ptr<AbstractRequestHandler>&& requestHandler)
    : mSslStream(SslStream::create(std::move(socket), context)),
      mResponseKeepAlive(),
      mSerializerKeepAlive(),
      mIsStreamClosedOrClosing(false),
      mRequestHandler(std::move(requestHandler))
{
}


// Start the asynchronous operation
void ServerSession::run (void)
{
    // Set the timeout.
    mSslStream.getLowestLayer().expires_after(std::chrono::seconds(ErpConstants::SocketTimeoutSeconds));

    // Perform the SSL handshake
    mSslStream.getSslStream().async_handshake(
        boost::asio::ssl::stream_base::server,
        boost::beast::bind_front_handler(
            &ServerSession::onTlsHandshakeComplete,
            shared_from_this()));
}


void ServerSession::onTlsHandshakeComplete (boost::beast::error_code ec)
{
    if (ec.value() != 0)
    {
        ErrorHandler(ec).reportServerError("handshake");
        switch(ec.value())
        {
            case 336130204:
                // This is an openssl error code which is used for HTTP only (no S) requests.
                // It is unknown if there is a symbolic definition for this error code somewhere.
                LOG(ERROR) << "HTTP requests without TLS are not supported";
                break;
        }
        // Regard any error at this stage as not recoverable.

        // In case the error is due to a HTTP request then there is not much use in sending a response
        // because we only speak HTTPS and the client doesn't. We could extend the server to be able to switch
        // to HTTP but that would be overkill for a server that is not directly exposed to the internet and only
        // speaks directly to internal services which also only understand HTTPS.
        sendResponse(getBadRequestResponse());
        do_close();
    }
    else
    {
        // Set the timeout.
        mSslStream.getLowestLayer().expires_after(std::chrono::seconds(ErpConstants::SocketTimeoutSeconds));

        do_read();
    }
}


void ServerSession::do_read (void)
{
    try
    {
        const auto [success, response] = mRequestHandler->handleRequest(
            mSslStream);

        // Send the response.
        A_20163.start("10 - send response back to web interface");
        sendResponse(response);
        A_20163.finish();

        if ( ! success)
            do_close();

        // TODO: close the connection if the outer request has header field "Connection: close". (https://dth01.ibmgcloud.net/jira/browse/ERP-4733)
    }
    catch (...)
    {
        // This catch is our last line of defense against unexpected exceptions killing the current thread or the
        // application.
        // If you have seen the following message in a logfile then please modify the implementation so that
        // the exception is caught earlier.
        LOG(ERROR) << "Caught exception. Exceptions should not reach this place. Please check the implementation";
        logException(std::current_exception());

        // Having said all that, the stream is still good and a response has not yet been sent. That means that we can
        // still respond with a generic error response. Note that it is not encrypted.
        sendResponse(getServerErrorResponse());
    }
}


void ServerSession::on_write(
    bool close,
    boost::beast::error_code ec,
    std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    if (ec == boost::beast::http::error::end_of_stream)
    {
        VLOG(1) << "on_write exit for end_of_stream";
        return do_close();
    }

    ErrorHandler(ec).reportServerError("ServerSession::on_write");

    if (close)
    {
        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        VLOG(2) << "on_write exit for close";
        return do_close();
    }

    // We're done with the response so delete it
    mResponseKeepAlive.reset();
    mSerializerKeepAlive.reset();

    // Read another request
    VLOG(2) << "ServerSession::on_write reading another request";

    do_read();
}


void ServerSession::do_close (void)
{
    // Set the timeout.
    mSslStream.getLowestLayer().expires_after(std::chrono::seconds(ErpConstants::SocketTimeoutSeconds));

    // Perform the SSL shutdown
    mIsStreamClosedOrClosing = true;
    mSslStream.getSslStream().async_shutdown(
        boost::beast::bind_front_handler(
            &ServerSession::on_shutdown,
            shared_from_this()));
}


void ServerSession::on_shutdown(boost::beast::error_code ec)
{
    ErrorHandler(ec).reportServerError("ServerSession::on_shutdown");

    // At this point the connection is closed gracefully
}


void ServerSession::sendResponse (const ServerResponse& response)
{
    try
    {
        ServerResponseWriter(ValidatedServerResponse(response)).write(mSslStream);
    }
    catch(...)
    {
        // We have already started to write a response. A part of it may have already been received by the caller.
        // Therefore we can not report the error with a HTTP error response.

        // Writing a log message is all we can do.
        LOG(ERROR) << "Caught exception while writing response.";
        logException(std::current_exception());
    }
}


ServerResponse ServerSession::getBadRequestResponse (void)
{
    ServerResponse response;
    response.setKeepAlive(false);
    response.setStatus(HttpStatus::BadRequest);
    response.setBody("");
    response.removeHeader(Header::ContentType);
    return response;
}


ServerResponse ServerSession::getNotFoundResponse (void)
{
    ServerResponse response;
    response.setKeepAlive(false);
    response.setStatus(HttpStatus::NotFound);
    response.setBody("");
    response.removeHeader(Header::ContentType);
    return response;
}


ServerResponse ServerSession::getServerErrorResponse(void)
{
    ServerResponse response;
    response.setKeepAlive(false);
    response.setStatus(HttpStatus::InternalServerError);
    response.setBody("");
    response.removeHeader(Header::ContentType);
    return response;
}


void ServerSession::logException(std::exception_ptr exception)
{
    try
    {
        // Rethrow `exception` to use C++'s type system to distinguish between different exception classes.
        if (exception)
            std::rethrow_exception(exception);
    }
    catch (const ErpException &e)
    {
        TVLOG(1) << "caught ErpException " << e.what();
    }
    catch (const JwtException &e)
    {
        TVLOG(1) << "caught JwtException " << e.what();
    }
    catch (const std::exception &e)
    {
        TVLOG(1) << "caught std::exception " << e.what();
    }
    catch (const boost::exception &e)
    {
        TVLOG(1) << "caught boost::exception " << boost::diagnostic_information(e);
    }
    catch (...)
    {
        TVLOG(1) << "caught throwable that is not derived from std::exception or boost::exception";
    }
}
