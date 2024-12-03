/*
* (C) Copyright IBM Deutschland GmbH 2021, 2024
* (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
*/

#include "exporter/server/HttpsServer.hxx"
#include "shared/server/ErrorHandler.hxx"
#include "shared/server/handler/RequestHandlerManager.hxx"
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/ssl.hpp>
#include <memory>

class ServerSocketHandler : public std::enable_shared_from_this<ServerSocketHandler>
{
public:
    ServerSocketHandler(boost::asio::io_context& ioContext, boost::asio::ssl::context& sslContext,
                        const boost::asio::ip::tcp::endpoint& endpoint, RequestHandlerManager&& requestHandlers,
                        MedicationExporterServiceContext& serviceContext);

    // Accept incoming connections on the server socket.
    void run(void);

private:
    boost::asio::io_context& mIoContext;
    boost::asio::ssl::context& mSslContext;
    boost::asio::ip::tcp::acceptor mAcceptor;
    boost::beast::flat_buffer mBuffer;
    RequestHandlerManager mRequestHandlers;
    MedicationExporterServiceContext& mServiceContext;

    void do_accept(void);
    void on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);
};

ServerSocketHandler::ServerSocketHandler(
   boost::asio::io_context& ioContext,
   boost::asio::ssl::context& sslContext,
   const boost::asio::ip::tcp::endpoint& endpoint,
   RequestHandlerManager&& requestHandlers,
   MedicationExporterServiceContext& serviceContext)
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

HttpsServer::HttpsServer(const std::string_view address, const uint16_t port, RequestHandlerManager&& requestHandlers,
                         MedicationExporterServiceContext& serviceContext, bool enforceClientAuthentication,
                         const SafeString& caCertificates)
    : BaseHttpsServer(address, port, enforceClientAuthentication, caCertificates)
    , mServiceContext(serviceContext)
{
    // Create and launch a listening port
    std::make_shared<ServerSocketHandler>(mThreadPool.ioContext(), mSslContext,
                                          boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(address), port),
                                          std::move(requestHandlers), mServiceContext)
        ->run();
    TVLOG(0) << "listening on port " << port;
}

MedicationExporterServiceContext& HttpsServer::serviceContext()
{
    return mServiceContext;
}
