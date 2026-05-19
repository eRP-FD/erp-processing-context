/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/util/GLog.hxx"

#include <functional>
#include <boost/asio.hpp>
#include <boost/beast.hpp>

class HttpServer
{
    using tcp = boost::asio::ip::tcp;

public:
    using RequestType = boost::beast::http::request<boost::beast::http::string_body>;
    using ResponseType = boost::beast::http::response<boost::beast::http::dynamic_body>;
    using RequestHandler = std::function<ResponseType(tcp::socket&, const RequestType&)>;

    HttpServer(const boost::asio::ip::address& address, uint16_t port, RequestHandler requestHandler)
        : mAddress{address}
        , mPort{port}
        , mRequestHandler{std::move(requestHandler)}
    {
    }

    void handleAccept()
    {
        mAcceptor.async_accept(make_strand(mAcceptor.get_executor()),
                               [this](boost::beast::error_code ec, tcp::socket s) {
                                   if (! ec)
                                   {
                                       std::make_shared<Session>(std::move(s), mRequestHandler)->start();
                                   }
                                   else
                                   {
                                       LOG(ERROR) << "Accept error: " << ec.message();
                                   }

                                   // handle next client
                                   handleAccept();
                               });
    }

    void run()
    {
        handleAccept();
        mIoContext.run();
    }

    void stop() {
        mIoContext.stop();
    };

private:
    boost::asio::ip::address mAddress;
    uint_least16_t mPort;
    boost::asio::io_context mIoContext{1};
    boost::asio::ip::tcp::acceptor mAcceptor{mIoContext, {mAddress, mPort}};
    RequestHandler mRequestHandler;


    struct Session : std::enable_shared_from_this<Session> {
        Session(tcp::socket s, RequestHandler handler)
            : socket(std::move(s))
            , mRequestHandler{handler}
        {
        }

        void start()
        {
            doRead();
        }


    private:
        void doRead()
        {
            namespace http = boost::beast::http;
            http::async_read(socket, buffer, request,
                             [this, self = shared_from_this()](boost::beast::error_code ec, size_t) mutable {
                                 if (! ec)
                                 {
                                     doResponse();
                                 }
                                 else
                                 {
                                     LOG(ERROR) << "Read error: " << ec.message();
                                 }
                             });
        }

        void doResponse()
        {
            namespace http = boost::beast::http;
            response = mRequestHandler(socket, request);
            response.prepare_payload();

            http::async_write(socket, response, [this, self = shared_from_this()](boost::beast::error_code ec, size_t) {
                socket.shutdown(tcp::socket::shutdown_send, ec); // NOLINT(bugprone-unused-return-value,cert-err33-c)
            });
        }
        tcp::socket socket;
        boost::beast::flat_buffer buffer{100000};
        boost::beast::http::request<boost::beast::http::string_body> request;
        boost::beast::http::response<boost::beast::http::dynamic_body> response;
        RequestHandler mRequestHandler;
    };
};
