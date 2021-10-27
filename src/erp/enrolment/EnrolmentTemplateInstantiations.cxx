/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/enrolment/EnrolmentServiceContext.hxx"
#include "erp/server/HttpsServer.ixx"
#include "erp/server/ServerSocketHandler.ixx"
#include "erp/server/context/SessionContext.ixx"
#include "erp/server/handler/RequestHandlerContext.ixx"
#include "erp/server/handler/RequestHandlerManager.ixx"
#include "erp/server/session/ServerSession.ixx"


template class ConcreteRequestHandler<EnrolmentServiceContext>;
template class HttpsServer<EnrolmentServiceContext>;
template class RequestHandlerContainer<EnrolmentServiceContext>;
template class RequestHandlerContext<EnrolmentServiceContext>;
template class RequestHandlerManager<EnrolmentServiceContext>;
template class ServerSocketHandler<EnrolmentServiceContext>;
template class SessionContext<EnrolmentServiceContext>;

template std::shared_ptr<ServerSession> ServerSession::createShared (
    boost::asio::ip::tcp::socket&& socket,
    boost::asio::ssl::context& context,
    const RequestHandlerManager<EnrolmentServiceContext>& requestHandlers,
    EnrolmentServiceContext& serviceContext);

