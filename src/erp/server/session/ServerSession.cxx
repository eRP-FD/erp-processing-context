/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/server/AccessLog.hxx"
#include "erp/server/ErrorHandler.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/server/handler/RequestHandlerManager.hxx"
#include "erp/server/response/ServerResponseWriter.hxx"
#include "erp/server/session/AsynchronousServerSession.hxx"
#include "erp/server/session/SynchronousServerSession.hxx"
#include "erp/util/ExceptionHelper.hxx"
#include "erp/util/JwtException.hxx"

#include <boost/exception/diagnostic_information.hpp>




namespace
{
[[maybe_unused]] // This seems to be necessary for CLion (clang-tidy?) to stop warning.
std::string getValidatedTarget (const Header& header)
{
    // Request path must be absolute and not contain "..".
    std::string target (header.target());
    if (target.empty()
        || target[0] != '/'
        || target.find("..") != std::string::npos)
    {
        TLOG(ERROR) << "handleRequest has found an error in target '" << target << "'";
        return {};
    }

    return target;
}
}


class ConcreteRequestHandler : public AbstractRequestHandler
{
public:
    const RequestHandlerManager& mRequestHandlers;
    PcServiceContext& mServiceContext;

    ConcreteRequestHandler (const RequestHandlerManager& requestHandlers, PcServiceContext& serviceContext);

    virtual std::tuple<bool,ServerResponse> handleRequest (
        ServerRequest&& request,
        AccessLog& accessLog);
};


std::shared_ptr<ServerSession> ServerSession::createShared(
    boost::asio::ip::tcp::socket&& socket,
    boost::asio::ssl::context& context,
    const RequestHandlerManager& requestHandlers,
    PcServiceContext& serviceContext)
{
    constexpr bool defaultToKeepAlive = false;

    if (Configuration::instance().getOptionalBoolValue(ConfigurationKey::SERVER_KEEP_ALIVE, defaultToKeepAlive))
        return std::make_shared<AsynchronousServerSession>(
            std::move(socket),
            context,
            std::make_unique<ConcreteRequestHandler>(requestHandlers, serviceContext));
    else
        return std::make_shared<SynchronousServerSession>(
            std::move(socket),
            context,
            std::make_unique<ConcreteRequestHandler>(requestHandlers, serviceContext));
}


ConcreteRequestHandler::ConcreteRequestHandler (
    const RequestHandlerManager& requestHandlers,
    PcServiceContext& serviceContext)
    : mRequestHandlers(requestHandlers),
    mServiceContext(serviceContext)
{
}


std::tuple<bool,ServerResponse> ConcreteRequestHandler::handleRequest (
    ServerRequest&& request,
    AccessLog& accessLog)
{
    const Header& header (request.header());
    const std::string target = getValidatedTarget(header);
    if (target.empty())
    {
        LOG(WARNING) << "invalid request target";
        TVLOG(1) << "invalid request target '" << header.target() << "'";
        return {false, ServerSession::getBadRequestResponse()};
    }

    TLOG(INFO) << "handling request to " << target;
    auto matchingHandler = mRequestHandlers.findMatchingHandler(header.method(), target);
    if (matchingHandler.handlerContext == nullptr)
    {
        LOG(WARNING) << "did not find handler for request";
        TVLOG(1) << "did not find a handler for " << header.method() << " " << target;
        return {false, ServerSession::getNotFoundResponse()};
    }

    request.setPathParameters(matchingHandler.handlerContext->pathParameterNames, matchingHandler.pathParameters);
    request.setQueryParameters(std::move(matchingHandler.queryParameters));
    request.setFragment(std::move(matchingHandler.fragment));

    // Hand over request processing to a request handler that has been registered for the method and path
    // of the request URL. In most cases that will be the VAU request handler that unwraps a TEE encrypted
    // request and then processes the inner HTTP request.
    // Other sources of incoming requests will be the HSM.
    ServerResponse response;
    SessionContext session (mServiceContext, request, response, accessLog);
    matchingHandler.handlerContext->handler->preHandleRequestHook(session);
    matchingHandler.handlerContext->handler->handleRequest(session);
    return {true, std::move(response)};
}

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
