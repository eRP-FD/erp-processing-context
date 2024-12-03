/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_SESSION_SERVERSESSION_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_SESSION_SERVERSESSION_HXX

#include "shared/server/BaseServerSession.hxx"

class PcServiceContext;
class RequestHandlerManager;


/**
 * Each ServerSession instance processes requests on a single socket connection.
 * The ServerSession supports keep alive, reading requests and writing responses is done asynchronously.

 */
class ServerSession : public BaseServerSession
{
public:
    static std::shared_ptr<ServerSession> createShared(boost::asio::ip::tcp::socket&& socket,
                                                       boost::asio::ssl::context& context,
                                                       const RequestHandlerManager& requestHandlers,
                                                       PcServiceContext& serviceContext);

    explicit ServerSession(boost::asio::ip::tcp::socket&& socket, boost::asio::ssl::context& context,
                           std::unique_ptr<AbstractRequestHandler>&& requestHandler, PcServiceContext& serviceContext);
    ~ServerSession() = default;

private:
    PcServiceContext& mServiceContext;
};

#endif
