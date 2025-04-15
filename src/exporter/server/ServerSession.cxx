/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/server/ServerSession.hxx"
#include "exporter/server/SessionContext.hxx"
#include "shared/server/handler/RequestHandlerInterface.hxx"
#include "shared/server/handler/RequestHandlerManager.hxx"
#include "shared/server/request/ServerRequestReader.hxx"

#include <boost/beast/http/write.hpp>

namespace exporter {

std::tuple<bool, std::optional<RequestHandlerManager::MatchingHandler>, ServerResponse> ExporterRequestHandler::handleRequest(ServerRequest& request, AccessLog& accessLog)
{
    auto result = PartialRequestHandler::handleRequest(request, accessLog);
    bool success = std::get<0>(result);
    if (not success)
    {
        return result;
    }
    std::optional<RequestHandlerManager::MatchingHandler> matchingHandler = std::get<1>(result);

    // Hand over request processing to a request handler that has been registered for the method and path
    // of the request URL. In most cases that will be the VAU request handler that unwraps a TEE encrypted
    // request and then processes the inner HTTP request.
    // Other sources of incoming requests will be the HSM.
    ServerResponse response;

    SessionContext session(mServiceContext, request, response, accessLog);
    matchingHandler.value().handlerContext->handler->preHandleRequestHook(session);
    matchingHandler.value().handlerContext->handler->handleRequest(session);

    return {success, std::nullopt, std::move(response)};
}


std::shared_ptr<ServerSession> ServerSession::createShared(
    boost::asio::ip::tcp::socket&& socket,
    boost::asio::ssl::context& context,
    const RequestHandlerManager& requestHandlers,
    MedicationExporterServiceContext& serviceContext)
{
    return std::make_shared<ServerSession>(
        std::move(socket),
        context,
        std::make_unique<ExporterRequestHandler>(requestHandlers, serviceContext));
}


ServerSession::ServerSession (
    boost::asio::ip::tcp::socket&& socket,
    boost::asio::ssl::context& context,
    std::unique_ptr<ExporterRequestHandler>&& requestHandler)
	: BaseServerSession(std::move(socket), context, std::move(requestHandler))
{
}

} // namespace exporter
