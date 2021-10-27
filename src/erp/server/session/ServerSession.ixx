/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/ErpConstants.hxx"
#include "erp/server/session/AsynchronousServerSession.hxx"
#include "erp/server/session/SynchronousServerSession.hxx"
#include "erp/server/request/ServerRequestReader.hxx"
#include "erp/tee/ErpTeeProtocol.hxx"

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


template<class ServiceContextType>
class ConcreteRequestHandler : public AbstractRequestHandler
{
public:
    const RequestHandlerManager<ServiceContextType>& mRequestHandlers;
    ServiceContextType& mServiceContext;

    ConcreteRequestHandler (const RequestHandlerManager<ServiceContextType>& requestHandlers, ServiceContextType& serviceContext);

    virtual std::tuple<bool,ServerResponse> handleRequest (
        ServerRequest&& request,
        AccessLog& accessLog);
};


template<class ServiceContextType>
std::shared_ptr<ServerSession> ServerSession::createShared(
    boost::asio::ip::tcp::socket&& socket,
    boost::asio::ssl::context& context,
    const RequestHandlerManager<ServiceContextType>& requestHandlers,
    ServiceContextType& serviceContext)
{
    constexpr bool defaultToKeepAlive = false;

    if (Configuration::instance().getOptionalBoolValue(ConfigurationKey::SERVER_KEEP_ALIVE, defaultToKeepAlive))
        return std::make_shared<AsynchronousServerSession>(
            std::move(socket),
            context,
            std::make_unique<ConcreteRequestHandler<ServiceContextType>>(requestHandlers, serviceContext));
    else
        return std::make_shared<SynchronousServerSession>(
            std::move(socket),
            context,
            std::make_unique<ConcreteRequestHandler<ServiceContextType>>(requestHandlers, serviceContext));
}


template<class ServiceContextType>
ConcreteRequestHandler<ServiceContextType>::ConcreteRequestHandler (
    const RequestHandlerManager<ServiceContextType>& requestHandlers,
    ServiceContextType& serviceContext)
    : mRequestHandlers(requestHandlers),
      mServiceContext(serviceContext)
{
}


template<class ServiceContextType>
std::tuple<bool,ServerResponse> ConcreteRequestHandler<ServiceContextType>::handleRequest (
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
