/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_COHTTPCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_COHTTPCLIENT_HXX

#include <boost/asio/awaitable.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>
#include <chrono>
#include <memory>

#include "shared/network/client/ConnectionParameters.hxx"

namespace epa
{
class BinaryBuffer;
class ClientTeeHandshake;
struct Tee3Context;
}

class Certificate;

/// @brief co-rotine based HTTPS Client
///
/// If the hostname resolves to multiple addresses, tries to connect in a round-robin manner.
class CoHttpsClient : public std::enable_shared_from_this<CoHttpsClient>
{
public:
    using Strand = boost::asio::strand<boost::asio::any_io_executor>;
    using Resolver = boost::asio::ip::tcp::resolver;
    using TcpSocket = boost::asio::ip::tcp::socket;
    using SslStream = boost::asio::ssl::stream<TcpSocket>;
    using Body = boost::beast::http::string_body;

    template<bool isRequest>
    using Message = boost::beast::http::message<isRequest, Body>;
    using Request = Message<true>;
    using Response = Message<false>;

    CoHttpsClient(std::shared_ptr<Strand> strand, const ConnectionParameters& params);

    CoHttpsClient(boost::asio::io_context& ioContext, const ConnectionParameters& params);

    virtual ~CoHttpsClient() noexcept;

    /// @brief provides SSL-Context for SslStream
    /// can be overridden by user to setup specfic SSL Parameters or callbacks
    virtual boost::asio::ssl::context createSslContext(const std::optional<std::string>& forcedCiphers);

    /// @brief resolve the hostname and if the connection or TLS handshake to any endpoint fails
    /// implements a round-robin logic until all endpoints are exhausted
    boost::asio::awaitable<boost::system::error_code> resolveAndConnect();
    /// @brief connect to the specified endpoint and perform a TLS handshake, no retries are attempted
    boost::asio::awaitable<boost::system::error_code> connectToEndpoint(boost::asio::ip::tcp::endpoint endpoint);

    virtual boost::asio::awaitable<void> close();
    virtual bool connected() const;

    /// @brief send HTTP request
    boost::asio::awaitable<boost::system::result<Response>> send(std::string xRequestId, Request request);

    const std::string& hostname() const;
    uint16_t port() const;
    /// @brief address currently used by round robin
    const boost::asio::ip::tcp::endpoint& currentEndpoint() const;
    Strand& strand() const;

    /// @brief set mandatory HTTPS header fields
    void setMandatoryFields(const std::string& xRequestId, Request&) const;

protected:
    boost::asio::awaitable<void> initRetry();

private:
    std::string mHostname;
    uint16_t mPort;
    std::chrono::milliseconds mConnectionTimeout;
    TlsCertificateVerifier mCertificateVerifier;
    /// trust the certificate subject or only consider the SAN values from the certificate
    bool mTrustCn;
    boost::asio::ssl::context mSslContext;
    std::shared_ptr<boost::asio::strand<boost::asio::any_io_executor>> mStrand;

    boost::asio::ip::tcp::endpoint mCurrentEndpoint;
    std::unique_ptr<TcpSocket> mTcpSocket;
    std::unique_ptr<SslStream> mSslStream;
};

#endif// ERP_PROCESSING_CONTEXT_COHTTPCLIENT_HXX
