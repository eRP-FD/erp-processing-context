/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/admin/AdminServer.hxx"
#include "erp/admin/AdminRequestHandler.hxx"
#include "erp/admin/AdminServiceContext.hxx"
#include "erp/server/HttpsServer.ixx"
#include "erp/server/ServerSocketHandler.ixx"
#include "erp/server/context/SessionContext.ixx"
#include "erp/server/handler/RequestHandlerContext.ixx"
#include "erp/server/handler/RequestHandlerManager.ixx"
#include "erp/server/session/ServerSession.ixx"

AdminServer::AdminServer(const std::string_view& adminInterface, const uint16_t adminPort,
                         RequestHandlerManager<AdminServiceContext>&& handlerManager,
                         std::unique_ptr<AdminServiceContext> serviceContext)
    : mServer(adminInterface, adminPort, std::move(handlerManager), std::move(serviceContext))
{
}

void AdminServer::addEndpoints(RequestHandlerManager<AdminServiceContext>& manager)
{
    manager.onPostDo("/admin/shutdown", std::make_unique<PostRestartHandler>());
}

HttpsServer<AdminServiceContext>& AdminServer::getServer(void)
{
    return mServer;
}


template class HttpsServer<AdminServiceContext>;
template class RequestHandlerContainer<AdminServiceContext>;
template class RequestHandlerContext<AdminServiceContext>;
template class RequestHandlerManager<AdminServiceContext>;
template class ServerSocketHandler<AdminServiceContext>;
template class SessionContext<AdminServiceContext>;
