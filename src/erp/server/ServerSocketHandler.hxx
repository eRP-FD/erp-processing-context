/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_SERVERSOCKETHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_SERVERSOCKETHANDLER_HXX

#include "erp/server/handler/RequestHandlerManager.hxx"

#include <memory>

#include <boost/asio/io_context.hpp>
#include <boost/beast/ssl.hpp>

class PcServiceContext;


class ServerSocketHandler
    : public std::enable_shared_from_this<ServerSocketHandler>
{
public:
    ServerSocketHandler(
        boost::asio::io_context& ioContext,
        boost::asio::ssl::context& sslContext,
        const boost::asio::ip::tcp::endpoint& endpoint,
        RequestHandlerManager&& requestHandlers,
        PcServiceContext& serviceContext);

    // Accept incoming connections on the server socket.
    void run (void);

private:
    boost::asio::io_context& mIoContext;
    boost::asio::ssl::context& mSslContext;
    boost::asio::ip::tcp::acceptor mAcceptor;
    boost::beast::flat_buffer mBuffer;
    RequestHandlerManager mRequestHandlers;
    PcServiceContext& mServiceContext;

    void do_accept (void);
    void on_accept (boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);
};

#endif
