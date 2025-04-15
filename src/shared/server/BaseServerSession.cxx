/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/server/BaseServerSession.hxx"

#include "shared/server/AccessLog.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/server/ErrorHandler.hxx"
#include "shared/server/request/ServerRequestReader.hxx"
#include "shared/server/response/ServerResponseWriter.hxx"
#include "shared/util/JwtException.hxx"
#include "shared/server/Worker.hxx"

#include <boost/beast/http/write.hpp>
#include <boost/exception/diagnostic_information.hpp>


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

    bool keepAlive = false;
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
} // anonymous namespace


/**
 * Data for a single request.
 */
class BaseServerSession
::SessionData
{
public:
    AccessLog accessLog;
};


BaseServerSession::BaseServerSession (
    boost::asio::ip::tcp::socket&& socket,
    boost::asio::ssl::context& context,
    std::unique_ptr<AbstractRequestHandler>&& requestHandler)
    : mSslStream(SslStream::create(std::move(socket), context)),
      mResponseKeepAlive(),
      mSerializerKeepAlive(),
      mRequestHandler(std::move(requestHandler))
{
}


// Start the asynchronous operation
void BaseServerSession::run (void)
{
    // Set the timeout.
    mSslStream.getLowestLayer().expires_after(std::chrono::seconds(ErpConstants::SocketTimeoutSeconds));

    // Perform the SSL handshake
    mSslStream.getSslStream().async_handshake(
        boost::asio::ssl::stream_base::server,
        try_handler([self = shared_from_this()](boost::beast::error_code ec) mutable { self->onTlsHandshakeComplete(ec);}));
}


ServerResponse BaseServerSession::getBadRequestResponse (void)
{
    ServerResponse response;
    response.setKeepAlive(false);
    response.setStatus(HttpStatus::BadRequest);
    response.setBody("");
    response.removeHeader(Header::ContentType);
    return response;
}


ServerResponse BaseServerSession::getNotFoundResponse (void)
{
    ServerResponse response;
    response.setKeepAlive(false);
    response.setStatus(HttpStatus::NotFound);
    response.setBody("");
    response.removeHeader(Header::ContentType);
    return response;
}


ServerResponse BaseServerSession::getServerErrorResponse(void)
{
    ServerResponse response;
    response.setKeepAlive(false);
    response.setStatus(HttpStatus::InternalServerError);
    response.setBody("");
    response.removeHeader(Header::ContentType);
    return response;
}


void BaseServerSession::onTlsHandshakeComplete (boost::beast::error_code ec)
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
            default:
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


void BaseServerSession::do_read ()
{
    DebugLog("do_read");

    try
    {
        // Read the encrypted request body.
        auto reader = std::make_shared<ServerRequestReader>(mSslStream);
        reader->readAsynchronously(
            try_handler([self = shared_from_this(), reader]
            (std::optional<ServerRequest>&& request, const std::exception_ptr& exception) mutable
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
                    // no request has been read.
                    // This could be due to keep-alive was requested but no more request follows
                    data->accessLog.discard();
                    self->do_close(std::move(data));
                }
                else
                {
                    Expect3(request.has_value(), "neither request nor exception is set", std::logic_error);
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
        TLOG(ERROR) << "Caught exception. Exceptions should not reach this place. Please check the implementation";
        logException(std::current_exception());

        // Having said all that, the stream is still good and a response has not yet been sent. That means that we can
        // still respond with a generic error response. Note that it is not encrypted.
        sendResponse(getServerErrorResponse());
    }
}


void BaseServerSession::on_read (ServerRequest request, SessionDataPointer&& data)
{
    DebugLog("on_read");

    do_handleRequest(std::move(request), std::move(data));
}


void BaseServerSession::do_handleRequest (ServerRequest request, SessionDataPointer&& data)
{
    DebugLog("do_handleRequest");

    try
    {
        setLogId(request.header().header(Header::XRequestId));
        data->accessLog.updateFromOuterRequest(request);
        const bool keepAlive = requestSupportsKeepAlive(request, data->accessLog);
        auto [success, matchingHandler, response] = mRequestHandler->handleRequest(request, data->accessLog);
        do_write(std::move(response), success&&keepAlive, std::move(data));
    }
    catch(...)
    {
        // The exception is expected to come from the request handler.
        // As any error that is caused by a bad request should have already been handled and turned into an error
        // response, any exception that makes it to this catch clause is interpreted as error in the implementation.
        data->accessLog.error("caught exception in BaseServerSession::do_handleRequest", std::current_exception());
        data->accessLog.keyValue("response-code", static_cast<size_t>(500));
        do_write(getServerErrorResponse(), false, std::move(data));
    }
}


void BaseServerSession::do_write (ServerResponse response, const bool keepConnectionAlive, SessionDataPointer&& data)
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
        try_handler([self = shared_from_this(), writer, keepConnectionAlive, data]
        (const bool success) mutable
        {
            self->on_write(keepConnectionAlive && success, std::move(data));
        }), &data->accessLog);
    A_20163.finish();
}


void BaseServerSession::on_write (const bool keepConnectionAlive, SessionDataPointer&& data)
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


void BaseServerSession::do_close (SessionDataPointer&& data)
{
    DebugLog("do_close");
    if (!mSslStream.getLowestLayer().socket().is_open())
    {
        TVLOG(3) << "Socket is already closed.";
        data.reset();
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


void BaseServerSession::on_shutdown (boost::beast::error_code ec, SessionDataPointer&& data)
{
    DebugLog("on_shutdown");
    if (mSslStream.getLowestLayer().socket().is_open())
    {
        mSslStream.getLowestLayer().socket().shutdown(boost::asio::socket_base::shutdown_both, ec);
    }

    data.reset();

    if (ec != boost::asio::error::not_connected)
    {
        ErrorHandler(ec).reportServerError("BaseServerSession::on_shutdown");
    }

    // At this point the connection has been closed gracefully.
}


template<typename HandlerT, typename ClassT, typename RetT, typename... ArgTs>
std::function<RetT(ArgTs...)> BaseServerSession::try_handler_detail(HandlerT&& handler,
                                                                RetT (ClassT::*)(ArgTs...))
{
    using erp::server::Worker;
    return [self = shared_from_this(), handler = std::forward<HandlerT>(handler),
            logContext = tlogContext](ArgTs&&... args) mutable {
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


void BaseServerSession::setLogId(const std::optional<std::string>& requestId)
{
    if (requestId.has_value())
    {
        TVLOG(0) << "Handling request with id: " << *requestId;
        tlogContext = requestId;
    }
    else
    {
        TVLOG(0) << "No ID in request.";
        setLogIdToRemote();
    }
}

void BaseServerSession::setLogIdToRemote()
{
    std::ostringstream endpointStrm;
    endpointStrm << mSslStream.getLowestLayer().socket().remote_endpoint();
    tlogContext = endpointStrm.str();
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void BaseServerSession::logException(const std::exception_ptr& exception)
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


void BaseServerSession::sendResponse (ServerResponse&& response, AccessLog* accessLog)
{
    try
    {
        ServerResponseWriter()
            .write(mSslStream, ValidatedServerResponse(std::move(response)), accessLog);
    }
    catch(...)
    {
        // We have already started to write a response. A part of it may have already been received by the caller.
        // Therefore we can not report the error with a HTTP error response.

        // Writing a log message is all we can do.
        TLOG(ERROR) << "Caught exception while writing response.";
        logException(std::current_exception());

        // Do not attempt to service another request.
    }
}
