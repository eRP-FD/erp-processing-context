#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/HttpsServer.ixx"
#include "erp/server/ServerSocketHandler.ixx"
#include "erp/server/context/SessionContext.ixx"
#include "erp/server/handler/RequestHandlerContext.ixx"
#include "erp/server/handler/RequestHandlerManager.ixx"
#include "erp/server/session/ServerSession.ixx"


template class ConcreteRequestHandler<PcServiceContext>;
template class HttpsServer<PcServiceContext>;
template class RequestHandlerContainer<PcServiceContext>;
template class RequestHandlerContext<PcServiceContext>;
template class RequestHandlerManager<PcServiceContext>;
template class ServerSocketHandler<PcServiceContext>;

template std::shared_ptr<ServerSession> ServerSession::createShared (
    boost::asio::ip::tcp::socket&& socket,
    boost::asio::ssl::context& context,
    const RequestHandlerManager<PcServiceContext>& requestHandlers,
    PcServiceContext& serviceContext);
