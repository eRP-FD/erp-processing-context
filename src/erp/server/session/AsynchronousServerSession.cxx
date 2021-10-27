/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/server/session/AsynchronousServerSession.hxx"

#include "erp/ErpConstants.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/server/AccessLog.hxx"
#include "erp/server/ErrorHandler.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/server/request/ServerRequestReader.hxx"
#include "erp/server/response/ServerResponseWriter.hxx"
#include "erp/tee/ErpTeeProtocol.hxx"
#include "erp/util/JwtException.hxx"

#include <boost/beast/http/write.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <chrono>

namespace
{
    /**
     * Return whether or not to keep the connection alive after the current request is finished.
     * This keeps into account the different default behaviors of different HTTP versions
     * and explicitly stated intent in the request header.
     */
    bool requestSupportsKeepAlive (
        const ServerRequest& request,
        AccessLog& accessLog)
    {
        std::optional<std::string> connectionValue;
        if (request.header().hasHeader(Header::Connection))
        {
            connectionValue =
                String::toLower(
                    String::trim(
                        *request.header().header(Header::Connection)));
        }

        bool keepAlive;
        if (request.header().version() >= Header::Version_1_1)
        {
            // Keep-Alive is enabled by default since http/1.1
            // but can be disabled with header "Connection: close"
            keepAlive = connectionValue.value_or("") != Header::ConnectionClose;
        }
        else
        {
            // Keep-Alive is disabled by default in http/1.0
            // but can be enabled with header "Connection: keep-alive"
            // The "keep-alive" header can supply further information like timeout values.
            // But is not yet supported.
            keepAlive = connectionValue.value_or("") == String::toLower(Header::KeepAlive);
        }

        accessLog.keyValue("keepAlive", "http" + std::to_string(request.header().version())
                                               + "/" + connectionValue.value_or("")
                                               + "/" + (keepAlive?"on":"off"));

        return keepAlive;
    }

// No debug logs by default. Switch to the commented out version when you want to debug the asynchronous implementation.
//#define DebugLog(message) TVLOG(1) << message
#define DebugLog(message)
}

template<typename HandlerT, typename ClassT, typename RetT, typename... ArgTs>
std::function<RetT(ArgTs...)> AsynchronousServerSession::try_handler_detail(HandlerT&& handler,
                                                                            RetT (ClassT::*)(ArgTs...))
{
    using erp::server::Worker;
    return [self = shared_from_this(), handler = std::forward<HandlerT>(handler),
            logContext = Worker::tlogContext](ArgTs&&... args) mutable {
        ScopedLogContext scope_log(logContext);
        try
        {
            handler(std::forward<ArgTs>(args)...);
        }
        catch (...)
        {
            self->logException(std::current_exception());
        }
    };
}


/**
 * Data for a single request.
 */
class AsynchronousServerSession::SessionData
{
public:
    AccessLog accessLog;
};



// Start the asynchronous operation
void AsynchronousServerSession::run (void)
{
    // Set the timeout.
    mSslStream.getLowestLayer().expires_after(std::chrono::seconds(ErpConstants::SocketTimeoutSeconds));

    // Perform the SSL handshake
    mSslStream.getSslStream().async_handshake(
        boost::asio::ssl::stream_base::server,
        try_handler([self = shared_from_this()](boost::beast::error_code ec) mutable { self->onTlsHandshakeComplete(ec);}));
}


void AsynchronousServerSession::onTlsHandshakeComplete (boost::beast::error_code ec)
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
                LOG(ERROR) << "HTTP requests without TLS are not supported";
                break;
        }
        // Regard any error at this stage as not recoverable.

        // In case the error is due to a HTTP request then there is not much use in sending a response
        // because we only speak HTTPS and the client doesn't. We could extend the server to be able to switch
        // to HTTP but that would be overkill for a server that is not directly exposed to the internet and only
        // speaks directly to internal services which also only understand HTTPS.
        sendResponse(getBadRequestResponse());
        do_close(std::make_unique<SessionData>());
    }
    else
    {
        // Set the timeout.
        mSslStream.getLowestLayer().expires_after(std::chrono::seconds(ErpConstants::SocketTimeoutSeconds));

        do_read();
    }
}


void AsynchronousServerSession::do_read ()
{
    DebugLog("do_read");

    try
    {
        // Read the encrypted request body.
        auto reader = std::make_shared<ServerRequestReader>(mSslStream);
        reader->readAsynchronously(
            try_handler([self = shared_from_this(), reader]
            (std::optional<ServerRequest>&& request, std::exception_ptr exception) mutable
            {
                auto data = std::make_unique<SessionData>();
                DebugLog("do_read callback");
                if (exception != nullptr)
                {
                    data->accessLog.error("reading request failed before header could be read", exception);
                    if ( ! reader->isStreamClosed())
                        self->do_write(getBadRequestResponse(), false, std::move(data));
                }
                else if (reader->isStreamClosed())
                {
                    self->do_close(std::move(data));
                }
                else
                {
                    Expect(request.has_value(), "neither request nor exception is set");
                    self->on_read(std::move(request.value()), std::move(data));
                }
            }));
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


void AsynchronousServerSession::on_read (ServerRequest request, SessionDataPointer&& data)
{
    DebugLog("on_read");

    do_handleRequest(std::move(request), std::move(data));
}


void AsynchronousServerSession::do_handleRequest (ServerRequest request, SessionDataPointer&& data)
{
    DebugLog("do_handleRequest");

    try
    {
        setLogId(request.header().header(Header::XRequestId));
        data->accessLog.updateFromOuterRequest(request);
        const bool keepAlive = requestSupportsKeepAlive(request, data->accessLog);
        auto [success, response] = mRequestHandler->handleRequest(std::move(request), data->accessLog);
        do_write(std::move(response), success&&keepAlive, std::move(data));
    }
    catch(...)
    {
        // The exception is expected to come from the request handler.
        // As any error that is caused by a bad request should have already been handled and turned into an error
        // response, any exception that makes it to this catch clause is interpreted as error in the implementation.
        data->accessLog.error("caught exception in ServerSession::do_handleRequest", std::current_exception());
        data->accessLog.keyValue("response-code", static_cast<size_t>(500));
        do_write(getServerErrorResponse(), false, std::move(data));
    }
}


void AsynchronousServerSession::do_write (ServerResponse response, const bool keepConnectionAlive, SessionDataPointer&& data)
{
    DebugLog("do_write");

    if ( ! keepConnectionAlive)
    {
        // Inform the client that we will close the connection after sending the response.
        response.setHeader(Header::Connection, Header::ConnectionClose);
    }

    auto writer = std::make_shared<ServerResponseWriter>();
    // Send the response.
    A_20163.start("10 - send response back to web interface");
    writer->writeAsynchronously(
        mSslStream,
        ValidatedServerResponse(std::move(response)),
        try_handler([self = shared_from_this(), writer, keepConnectionAlive, data = std::move(data)]
        (const bool success) mutable
        {
            self->on_write(keepConnectionAlive && success, std::move(data));
        }));
    A_20163.finish();
}


void AsynchronousServerSession::on_write (const bool keepConnectionAlive, SessionDataPointer&& data) noexcept
{
    DebugLog("on_write");

    if (keepConnectionAlive)
    {
        // Life time of the session data ends here. do_read will create a new one for another request.
        data.reset();

        boost::asio::post(
            mSslStream.get_executor(),
            try_handler([self=shared_from_this()] () mutable
            {
                DebugLog("on_write callback");
                self->do_read();
            }));
    }
    else
    {
        do_close(std::move(data));
    }
}


void AsynchronousServerSession::do_close (SessionDataPointer&& data)
{
    DebugLog("do_close");
    if (!mSslStream.getLowestLayer().socket().is_open())
    {
        TVLOG(3) << "Socket is already closed.";
        on_shutdown({}, std::move(data));
        return;
    }

    // Set the timeout.
    mSslStream.getLowestLayer().expires_after(std::chrono::seconds(ErpConstants::SocketTimeoutSeconds));

    // Perform the SSL shutdown
    mIsStreamClosedOrClosing = true;
    mSslStream.getSslStream().async_shutdown(
        try_handler([self = shared_from_this(), data = std::move(data)]
        (boost::beast::error_code ec) mutable
        {
            self->on_shutdown(ec, std::move(data));
        }));
}


void AsynchronousServerSession::on_shutdown (boost::beast::error_code ec, SessionDataPointer&& data)
{
    DebugLog("on_shutdown");

    mSslStream.getLowestLayer().socket().shutdown(boost::asio::socket_base::shutdown_both, ec);

    data.reset();

    if (ec != boost::asio::error::not_connected)
    {
        ErrorHandler(ec).reportServerError("ServerSession::on_shutdown");
    }

    // At this point the connection has been closed gracefully.
}


