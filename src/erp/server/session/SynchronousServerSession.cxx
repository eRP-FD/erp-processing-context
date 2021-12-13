/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/server/session/SynchronousServerSession.hxx"

#include "erp/ErpConstants.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/server/AccessLog.hxx"
#include "erp/server/ErrorHandler.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/server/request/ServerRequestReader.hxx"
#include "erp/server/response/ServerResponseWriter.hxx"
#include "erp/server/response/ValidatedServerResponse.hxx"
#include "erp/tee/ErpTeeProtocol.hxx"
#include "erp/util/JwtException.hxx"
#include "erp/util/TLog.hxx"

#include <boost/beast/http/write.hpp>
#include <boost/exception/diagnostic_information.hpp>

//#define DebugLog(message) TVLOG(1) << message
#define DebugLog(message)


void SynchronousServerSession::run (void)
{
    // Set the timeout.
    mSslStream.getLowestLayer().expires_after(std::chrono::seconds(ErpConstants::SocketTimeoutSeconds));

    // Perform the SSL handshake
    mSslStream.getSslStream().async_handshake(
        boost::asio::ssl::stream_base::server,
        boost::beast::bind_front_handler(
            &SynchronousServerSession::onTlsHandshakeComplete,
            shared_from_this()));
}


void SynchronousServerSession::onTlsHandshakeComplete (boost::beast::error_code ec)
{
    DebugLog("onTlsHandshakeComplete");

    if (ec.value() != 0)
    {
        ErrorHandler(ec).reportServerError("handshake");
        switch(ec.value())
        {
            case 336130204:
                // This is an openssl error code which is used for HTTP only (no S) requests.
                // It is unknown if there is a symbolic definition for this error code somewhere.
                TLOG(ERROR) << "HTTP requests without TLS are not supported";
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


void SynchronousServerSession::do_read (void)
{
    DebugLog("do_read");
    AccessLog accessLog;

    try
    {
        auto reader = ServerRequestReader(mSslStream);
        auto request = reader.read();
        // Read the encrypted request body.
        if (reader.isStreamClosed())
        {
            accessLog.error("stream is closed, reading request failed");
            sendResponse(getBadRequestResponse(), &accessLog);
        }
        else
        {
            setLogId(request.header().header(Header::XRequestId));
            accessLog.updateFromOuterRequest(request);
            auto [success, response] = mRequestHandler->handleRequest(std::move(request), accessLog);
            response.setKeepAlive(false); // Keep alive is not supported by the SynchronousServerSession.

            // Send the response.
            A_20163.start("10 - send response back to web interface");
            sendResponse(std::move(response), &accessLog);
            A_20163.finish();
        }

        // Keep alive is not supported.
        do_close();
    }
    catch (...)
    {
        // This catch is our last line of defense against unexpected exceptions killing the current thread or the
        // application.
        // If you have seen the following message in a logfile then please modify the implementation so that
        // the exception is caught earlier.
        TLOG(ERROR) << "Caught exception. Exceptions should not reach this place. Please check the implementation";
        logException(std::current_exception());
        accessLog.error("caught exception in SynchronousServerSession::do_read", std::current_exception());

        // Having said all that, the stream is still good and a response has not yet been sent. That means that we can
        // still respond with a generic error response. Note that it is not encrypted.
        sendResponse(getServerErrorResponse(), &accessLog);
    }
}


void SynchronousServerSession::on_write(
    bool close,
    boost::beast::error_code ec,
    std::size_t bytes_transferred)
{
    DebugLog("on_write");

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


void SynchronousServerSession::do_close (void)
{
    DebugLog("do_close");

    // Set the timeout.
    mSslStream.getLowestLayer().expires_after(std::chrono::seconds(ErpConstants::SocketTimeoutSeconds));

    // Perform the SSL shutdown
    mIsStreamClosedOrClosing = true;
    mSslStream.getSslStream().async_shutdown(
        boost::beast::bind_front_handler(
            &SynchronousServerSession::on_shutdown,
            shared_from_this()));
}


void SynchronousServerSession::on_shutdown(boost::beast::error_code ec)
{
    DebugLog("on_shutdown");

    ErrorHandler(ec).reportServerError("ServerSession::on_shutdown");

    // At this point the connection is closed gracefully
}
