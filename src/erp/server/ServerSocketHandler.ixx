/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/enrolment/EnrolmentServiceContext.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/pc/pre_user_pseudonym/PreUserPseudonymManager.hxx"
#include "erp/server/ErrorHandler.hxx"
#include "erp/server/ServerSocketHandler.hxx"
#include "erp/server/session/SynchronousServerSession.hxx"

#include <boost/asio/strand.hpp>


template<class ServiceContextType>
ServerSocketHandler<ServiceContextType>::ServerSocketHandler(
    boost::asio::io_context& ioContext,
    boost::asio::ssl::context& sslContext,
    const boost::asio::ip::tcp::endpoint& endpoint,
    RequestHandlerManager<ServiceContextType>&& requestHandlers,
    std::shared_ptr<ServiceContextType> serviceContext)
    : mIoContext(ioContext),
      mSslContext(sslContext),
      mAcceptor(boost::asio::make_strand(mIoContext)),
      mRequestHandlers(std::move(requestHandlers)),
      mServiceContext(std::move(serviceContext))
{
    ErrorHandler eh;

    // Open the acceptor
    mAcceptor.open(endpoint.protocol(), eh.ec);
    eh.throwOnServerError("ServerSocketHandler open");

    // Allow address reuse
    mAcceptor.set_option(boost::asio::socket_base::reuse_address(true), eh.ec);
    eh.throwOnServerError("ServerSocketHandler set_option");

    // Bind to the server address
    mAcceptor.bind(endpoint, eh.ec);
    eh.throwOnServerError("ServerSocketHandler bind");

    // Start listening for connections
    mAcceptor.listen(
        boost::asio::socket_base::max_listen_connections, eh.ec);
    eh.throwOnServerError("ServerSocketHandler listen");
}


template<class ServiceContextType>
void ServerSocketHandler<ServiceContextType>::run (void)
{
    do_accept();
}


template<class ServiceContextType>
void ServerSocketHandler<ServiceContextType>::do_accept (void)
{
    // The new connection gets its own strand
    mAcceptor.async_accept(
        boost::asio::make_strand(mIoContext),
        boost::beast::bind_front_handler(
            &ServerSocketHandler::on_accept,
            this->shared_from_this()));
}


template<class ServiceContextType>
void ServerSocketHandler<ServiceContextType>::on_accept (boost::beast::error_code ec, boost::asio::ip::tcp::socket socket)
{
    if (ec)
    {
        ErrorHandler(ec).throwOnServerError("on_accept");
    }
    else
    {
        // Create a new ServerSession and run it
        ServerSession::createShared(
            std::move(socket),
            mSslContext,
            mRequestHandlers,
            *mServiceContext)->run();
    }

    // Accept another connection
    do_accept();
}
