/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_SESSION_SERVERSESSION_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_SESSION_SERVERSESSION_HXX

#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/server/SslStream.hxx"

#include <functional>


class RequestHandlerContext;
class RequestHandlerManager;
class AccessLog;
class PcServiceContext;


class AbstractRequestHandler
{
public:
    virtual ~AbstractRequestHandler (void) = default;

    virtual std::tuple<bool,ServerResponse> handleRequest (
        ServerRequest&& request,
        AccessLog& log) = 0;
};


/**
 * Each ServerSession instance processes requests on a single socket connection.
 * The ServerSession supports keep alive, reading requests and writing responses is done asynchronously.

 */
class ServerSession : public std::enable_shared_from_this<ServerSession>
{
public:
    class SessionData;
    using SessionDataPointer = std::shared_ptr<SessionData>;

    static std::shared_ptr<ServerSession> createShared (
        boost::asio::ip::tcp::socket&& socket,
        boost::asio::ssl::context& context,
        const RequestHandlerManager& requestHandlers,
        PcServiceContext& serviceContext);

    explicit ServerSession(
        boost::asio::ip::tcp::socket&& socket,
        boost::asio::ssl::context& context,
        std::unique_ptr<AbstractRequestHandler>&& requestHandler);
    ~ServerSession() = default;

    void run();

    static ServerResponse getBadRequestResponse();
    static ServerResponse getNotFoundResponse();
    static ServerResponse getServerErrorResponse();

private:
    void onTlsHandshakeComplete (boost::beast::error_code ec);
    void do_read ();
    void on_read (ServerRequest request, SessionDataPointer&& data);
    void do_handleRequest (ServerRequest request, SessionDataPointer&& data);
    void do_write (ServerResponse response, const bool keepConnectionAlive, SessionDataPointer&& data);
    void on_write (const bool keepConnectionAlive, SessionDataPointer&& data);
    void do_close (SessionDataPointer&& data);
    void on_shutdown (boost::beast::error_code ec, SessionDataPointer&& data);

    template<typename HandlerT, typename ClassT, typename RetT, typename... ArgTs>
    inline std::function<RetT(ArgTs...)> try_handler_detail(HandlerT&& handler, RetT (ClassT::*)(ArgTs...));

    template<typename HandlerT>
    auto try_handler(HandlerT&& handler)
    {
        return try_handler_detail(std::forward<HandlerT>(handler), &HandlerT::operator());
    }

    void setLogId(const std::optional<std::string>& requestId);
    void setLogIdToRemote();

    SslStream mSslStream;
    std::shared_ptr<void> mResponseKeepAlive;
    std::shared_ptr<void> mSerializerKeepAlive;
    /// This flag is set to true when mSslStream is being closed and therefore should not be used anymore for
    /// reading and writing. It remains set to true when the stream is closed.
    bool mIsStreamClosedOrClosing;

    std::unique_ptr<AbstractRequestHandler> mRequestHandler;

    void logException(const std::exception_ptr& exception);
    void sendResponse (ServerResponse&& response, AccessLog* accessLog = nullptr);
};

#endif
