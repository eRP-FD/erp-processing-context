/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/ErpConstants.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/server/AccessLog.hxx"
#include "erp/server/ErrorHandler.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/server/request/ServerRequestReader.hxx"
#include "erp/server/response/ServerResponseWriter.hxx"
#include "erp/server/session/SynchronousServerSession.hxx"
#include "erp/tee/ErpTeeProtocol.hxx"
#include "erp/util/ExceptionHelper.hxx"
#include "erp/util/JwtException.hxx"

#include <boost/beast/http/write.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <chrono>


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

void ServerSession::setLogId(const std::optional<std::string>& requestId)
{
    if (requestId.has_value())
    {
        TLOG(INFO) << "Handling request with id: " << *requestId;
        erp::server::Worker::tlogContext = requestId;
    }
    else
    {
        TLOG(WARNING) << "No ID in request.";
        setLogIdToRemote();
    }
}

void ServerSession::setLogIdToRemote()
{
    std::ostringstream endpointStrm;
    endpointStrm << mSslStream.getLowestLayer().socket().remote_endpoint();
    erp::server::Worker::tlogContext = endpointStrm.str();
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


void ServerSession::sendResponse (ServerResponse&& response, AccessLog* accessLog)
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
