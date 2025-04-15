/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/server/ServerSession.hxx"
#include "erp/server/RequestHandler.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/server/AccessLog.hxx"
#include "shared/server/ErrorHandler.hxx"
#include "shared/server/Worker.hxx"
#include "shared/server/request/ServerRequestReader.hxx"
#include "shared/server/response/ServerResponseWriter.hxx"
#include "shared/util/JwtException.hxx"

#include <boost/beast/http/write.hpp>
#include <boost/exception/diagnostic_information.hpp>


std::shared_ptr<ServerSession> ServerSession::createShared(boost::asio::ip::tcp::socket&& socket,
                                                           boost::asio::ssl::context& context,
                                                           const RequestHandlerManager& requestHandlers,
                                                           PcServiceContext& serviceContext)
{
    return std::make_shared<ServerSession>(
        std::move(socket), context, std::make_unique<RequestHandler>(requestHandlers, serviceContext), serviceContext);
}


ServerSession::ServerSession(boost::asio::ip::tcp::socket&& socket, boost::asio::ssl::context& context,
                             std::unique_ptr<AbstractRequestHandler>&& requestHandler, PcServiceContext& serviceContext)
    : BaseServerSession(std::move(socket), context, std::move(requestHandler))
    , mServiceContext(serviceContext)
{
}
