/*
* (C) Copyright IBM Deutschland GmbH 2021, 2023
* (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
*/

#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/ErrorHandler.hxx"
#include "erp/server/ServerSocketHandler.hxx"
#include "erp/server/ServerSession.hxx"

#include <boost/asio/strand.hpp>


ServerSocketHandler::ServerSocketHandler(
   boost::asio::io_context& ioContext,
   boost::asio::ssl::context& sslContext,
   const boost::asio::ip::tcp::endpoint& endpoint,
   RequestHandlerManager&& requestHandlers,
   PcServiceContext& serviceContext)
   : mIoContext(ioContext),
   mSslContext(sslContext),
   mAcceptor(boost::asio::make_strand(mIoContext)),
   mRequestHandlers(std::move(requestHandlers)),
   mServiceContext(serviceContext)
{
   ErrorHandler eh;

   // Open the acceptor
   mAcceptor.open(endpoint.protocol(), eh.ec);
   eh.throwOnServerError("ServerSocketHandler open", {__FILE__, __LINE__});

   // Allow address reuse
   mAcceptor.set_option(boost::asio::socket_base::reuse_address(true), eh.ec);
   eh.throwOnServerError("ServerSocketHandler set_option", {__FILE__, __LINE__});

   // Bind to the server address
   mAcceptor.bind(endpoint, eh.ec);
   eh.throwOnServerError("ServerSocketHandler bind", {__FILE__, __LINE__});

   // Start listening for connections
   mAcceptor.listen(
       boost::asio::socket_base::max_listen_connections, eh.ec);
   eh.throwOnServerError("ServerSocketHandler listen", {__FILE__, __LINE__});
}


void ServerSocketHandler::run (void)
{
   do_accept();
}


void ServerSocketHandler::do_accept (void)
{
   // The new connection gets its own strand
   mAcceptor.async_accept(
       boost::asio::make_strand(mIoContext),
       boost::beast::bind_front_handler(
           &ServerSocketHandler::on_accept,
           this->shared_from_this()));
}


void ServerSocketHandler::on_accept (boost::beast::error_code ec, boost::asio::ip::tcp::socket socket)
{
   std::ostringstream endpointStrm;
   endpointStrm << socket.remote_endpoint();
   ScopedLogContext scopeLog{endpointStrm.str()};
   TVLOG(0) << "Accepted connection";
   socket.async_wait(boost::asio::ip::tcp::socket::wait_error,
                     [endpoint = socket.remote_endpoint()](boost::system::error_code ec){
                         if (ec && ec != boost::asio::error::operation_aborted)
                         {
                             TLOG(WARNING) << "connection " << endpoint << ": " << ec.message();
                         }
                         else
                         {
                             TVLOG(2) << "connection closed: " << endpoint;
                         }
                     });
   if (ec)
   {
       ErrorHandler(ec).throwOnServerError("on_accept", {__FILE__, __LINE__});
   }
   else
   {
       // Create a new ServerSession and run it
       ServerSession::createShared(
           std::move(socket),
           mSslContext,
           mRequestHandlers,
           mServiceContext)->run();
   }

   // Accept another connection
   do_accept();
}
