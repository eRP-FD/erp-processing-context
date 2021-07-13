#ifndef ERP_PROCESSING_CONTEXT_SERVER_SESSION_SERVERSESSION_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_SESSION_SERVERSESSION_HXX

#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/server/SslStream.hxx"


template<class ServiceContextType>
class RequestHandlerContext;

template<class ServiceContextType>
class RequestHandlerManager;

class AbstractRequestHandler
{
public:
    virtual ~AbstractRequestHandler (void) = default;

    virtual std::tuple<bool,ServerResponse> handleRequest (SslStream& sslStream) = 0;
};


/**
 * Each ServerSession instance processes requests on a single socket connection.
 */
class ServerSession
    : public std::enable_shared_from_this<ServerSession>
{
public:
    template<class ServiceContextType>
    static std::shared_ptr<ServerSession> createShared (
        boost::asio::ip::tcp::socket&& socket,
        boost::asio::ssl::context& context,
        const RequestHandlerManager<ServiceContextType>& requestHandlers,
        ServiceContextType& serviceContext);

    explicit ServerSession(
        boost::asio::ip::tcp::socket&& socket,
        boost::asio::ssl::context& context,
        std::unique_ptr<AbstractRequestHandler>&& requestHandler);

    void run (void);

    static ServerResponse getBadRequestResponse (void);
    static ServerResponse getNotFoundResponse (void);
    static ServerResponse getServerErrorResponse (void);

private:
    SslStream mSslStream;
    std::shared_ptr<void> mResponseKeepAlive;
    std::shared_ptr<void> mSerializerKeepAlive;
    /// This flag is set to true when mSslStream is being closed and therefore should not be used anymore for
    /// reading and writing. It remains set to true when the stream is closed.
    bool mIsStreamClosedOrClosing;

    std::unique_ptr<AbstractRequestHandler> mRequestHandler;

    void onTlsHandshakeComplete (boost::beast::error_code ec);
    void do_read (void);
    void on_write (
        bool close,
        boost::beast::error_code ec,
        std::size_t bytes_transferred);
    void do_close (void);
    void on_shutdown (boost::beast::error_code ec);

    void sendResponse (const ServerResponse& response);

    void logException(std::exception_ptr exception);
};

#endif
