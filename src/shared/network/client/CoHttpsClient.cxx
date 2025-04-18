/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */


#include "shared/network/client/CoHttpsClient.hxx"

#include "shared/erp-serverinfo.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>



using boost::asio::as_tuple;
using boost::asio::deferred;

CoHttpsClient::CoHttpsClient(std::shared_ptr<Strand> strand, const ConnectionParameters& params)
    : mHostname{params.hostname}
    , mPort{gsl::narrow<uint16_t>(std::stoi(params.port))}
    , mConnectionTimeout{params.connectionTimeout}
    , mCertificateVerifier{params.tlsParameters.value().certificateVerifier}
    , mTrustCn{params.tlsParameters.value().trustCertificateCn}
    , mSslContext{createSslContext(params.tlsParameters.value().forcedCiphers)}
    , mStrand{std::move(strand)}
{
    Expect3(mStrand != nullptr, "strand must not be nullptr.", std::logic_error);
    Expect(mConnectionTimeout.count() > 0, "Connection timeout must be greater than 0.");
}

CoHttpsClient::CoHttpsClient(boost::asio::io_context& ioContext, const ConnectionParameters& params)
    : CoHttpsClient{std::make_shared<Strand>(make_strand(ioContext)), params}
{

}


CoHttpsClient::~CoHttpsClient() noexcept = default;


boost::asio::ssl::context CoHttpsClient::createSslContext(const std::optional<std::string>& forcedCiphers)
{
    auto tlsContext = boost::asio::ssl::context{boost::asio::ssl::context::tlsv12_client};
    mCertificateVerifier.install(tlsContext);
    tlsContext.set_options(boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::no_sslv3 |
                           boost::asio::ssl::context::no_tlsv1 | boost::asio::ssl::context::no_tlsv1_1);
    // GEMREQ-start A_15751-03
    if (1 != SSL_CTX_set_cipher_list(tlsContext.native_handle(),
                                     forcedCiphers
                                         .value_or("ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES128-GCM-SHA256:"
                                                   "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256")
                                         .c_str()))
    {
        throw ExceptionWrapper<boost::beast::system_error>::create(
            {__FILE__, __LINE__}, static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category());
    }
    if (1 != SSL_CTX_set1_curves_list(tlsContext.native_handle(), "brainpoolP256r1:"
                                                                  "brainpoolP384r1:"
                                                                  "prime256v1:"
                                                                  "secp384r1"))
    {
        throw ExceptionWrapper<boost::beast::system_error>::create(
            {__FILE__, __LINE__}, static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category());
    }
    // Set allowed signature hash algorithms according to allowed cipher suites above
    if (1 != SSL_CTX_set1_sigalgs_list(tlsContext.native_handle(), "ECDSA+SHA384:"
                                                                   "ECDSA+SHA256:"
                                                                   "RSA+SHA384:"
                                                                   "RSA+SHA256"))
    {
        throw ExceptionWrapper<boost::beast::system_error>::create(
            {__FILE__, __LINE__}, static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category());
    }
    // GEMREQ-end A_15751-03
    SSL_CTX_set_info_callback(tlsContext.native_handle(), [](const SSL*, int context, int value)
    {
        if (context & SSL_CB_ALERT)
        {
            TVLOG(3) << ((context & SSL_CB_WRITE) ? "wrote" : "read")
                    << " TLS alert = "
                    << SSL_alert_desc_string_long(value)
                    << ", of type = "
                    << SSL_alert_type_string_long(value);
        }
    });
    return tlsContext;
}


boost::asio::awaitable<boost::system::error_code> CoHttpsClient::connectToEndpoint(boost::asio::ip::tcp::endpoint endpoint)
{
    mTcpSocket = std::make_unique<TcpSocket>(*mStrand);
    auto [ec] = co_await mTcpSocket->async_connect(endpoint, cancel_after(mConnectionTimeout, as_tuple(deferred)));
    if (ec)
    {
        if (ec == boost::system::errc::operation_canceled)
        {
            ec = boost::asio::error::timed_out;
        }
        TLOG(INFO) << "Connection to " << endpoint.address() << ":" << endpoint.port() << " failed: " << ec.message();
    }
    mCurrentEndpoint = endpoint;
    mSslStream = std::make_unique<SslStream>(std::move(*mTcpSocket), mSslContext);
    if (! SSL_clear(mSslStream->native_handle()))
    {
        throw ExceptionWrapper<boost::beast::system_error>::create(
            {__FILE__, __LINE__}, static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category());
    }
    // Set SNI Hostname (many hosts need this to handshake successfully).
    if (! SSL_set_tlsext_host_name(mSslStream->native_handle(), mHostname.c_str()))
    {
        throw ExceptionWrapper<boost::beast::system_error>::create(
            {__FILE__, __LINE__}, static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category());
    }
    // Verify the remote server's certificate's subject alias names.
    if (! SSL_set1_host(mSslStream->native_handle(), mHostname.c_str()))
    {
        throw ExceptionWrapper<boost::beast::system_error>::create(
            {__FILE__, __LINE__}, static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category());
    }
    if (! mTrustCn)
    {
        SSL_set_hostflags(mSslStream->native_handle(), X509_CHECK_FLAG_NEVER_CHECK_SUBJECT);
    }

    boost::asio::cancellation_signal signal;
    std::tie(ec) = co_await mSslStream->async_handshake(
        boost::asio::ssl::stream_base::client, cancel_after(mConnectionTimeout, as_tuple(deferred)));
    if (ec.category() == boost::asio::error::ssl_category)
    {
        TLOG(ERROR) << "Error during TLS handshake on " << mHostname << ":" << mPort << " (" << endpoint.address()
                    << "): " << ec.message();
    }
    co_return ec;
}

boost::asio::awaitable<boost::system::error_code> CoHttpsClient::resolveAndConnect()
{
    static const size_t maxConnectTriesPerAddress = static_cast<size_t>(Configuration::instance().getOptionalIntValue(ConfigurationKey::MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_RETRIES_PER_ADDRESS, 3));
    static const auto retryTimeout = std::chrono::milliseconds{Configuration::instance().getOptionalIntValue(ConfigurationKey::MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_RETRY_TIMEOUT_MILLISECONDS, 3000)};
    auto resolver = Resolver{*mStrand};
    auto [ec, addresses] = co_await resolver.async_resolve(mHostname, std::to_string(mPort), as_tuple(deferred));
    if (ec) {
        TLOG(INFO) << "host resolution failure: " << ec.message() << ": " << mHostname;
        co_return ec;
    }
    size_t connectTries = 0;
    do
    {
        boost::system::error_code timeoutEc;
        auto connectIdx = static_cast<ptrdiff_t>(connectTries % addresses.size());
        if (connectTries > 1) {
            co_await close();
            if (connectIdx == 0)
            {
                // delay when we've tried all endpoints once
                boost::asio::steady_timer timer{*mStrand, retryTimeout};
                std::tie(timeoutEc) = co_await timer.async_wait(as_tuple(deferred));
                if (timeoutEc.failed())
                {
                    TLOG(INFO) << "tee 3 client stopping connect: " << timeoutEc.message() << ": " << mHostname << ":"
                               << mPort;
                }
            }
        }
        if (! timeoutEc)
        {
            auto endpoint = *std::ranges::next(addresses.begin(), connectIdx, addresses.end());
            ec = co_await connectToEndpoint(endpoint);
        }
        else
        {
            ec = timeoutEc;
        }
        connectTries++;
    } while(connectTries < addresses.size() * maxConnectTriesPerAddress && ec.failed());
    if (ec)
    {
        TLOG(INFO) << "maximum number of tries (" << addresses.size() * maxConnectTriesPerAddress
                   << ") to host exhausted: " << mHostname;
    }

    co_return ec;
}


boost::asio::awaitable<boost::system::result<CoHttpsClient::Response>>
CoHttpsClient::send(std::string xRequestId, Request request)
{
    using namespace std::string_literals;
    static const auto fullMessageSendTimeout = std::chrono::milliseconds{Configuration::instance().getOptionalIntValue(ConfigurationKey::MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_MESSAGE_SEND_TIMEOUT_MILLISECONDS, 3000)};
    static const auto fullMessageReceiveTimeout = std::chrono::milliseconds{Configuration::instance().getOptionalIntValue(ConfigurationKey::MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_MESSAGE_RECEIVE_MILLISECONDS, 3000)};
    // This is done in a single function to avoid extra co-routine context creation
    if (! mSslStream)
    {
        co_return boost::asio::error::not_connected;
    }
    // send request
    {
        setMandatoryFields(xRequestId, request);
        boost::system::error_code ec;
        std::tie(ec, std::ignore) =
            co_await async_write(*mSslStream, request, cancel_after(fullMessageSendTimeout, as_tuple(deferred)));
        if (ec.failed())
        {
            if (ec == boost::system::errc::operation_canceled)
            {
                ec = boost::asio::error::timed_out;
            }
            LOG(INFO) << "send HTTP-Message failed: " << ec.message() << ": connecting to " << mHostname << " on "
                      << mCurrentEndpoint;
            co_return ec;
        }
    }

    // receive response
    Response response;
    {
        boost::system::error_code ec;
        std::string buffer;
        boost::asio::dynamic_string_buffer dynBuf{buffer};
        std::tie(ec, std::ignore) = co_await async_read(*mSslStream, dynBuf, response,
                                                        cancel_after(fullMessageReceiveTimeout, as_tuple(deferred)));
        if (ec.failed())
        {
            if (ec == boost::system::errc::operation_canceled)
            {
                ec = boost::asio::error::timed_out;
            }
            LOG(INFO) << "receive HTTP-Message failed: " << ec.message() << ": connecting to " << mHostname << " on "
                      << mCurrentEndpoint;
            co_return ec;
        }
    }
    co_return response;
}

boost::asio::awaitable<void> CoHttpsClient::close()
{
    using namespace std::chrono_literals;
    if (mSslStream)
    {
        auto executor = co_await boost::asio::this_coro::executor;
        TVLOG(2) << "shutdown TLS session";
        mSslStream->next_layer().cancel();
        co_await mSslStream->async_shutdown(cancel_after(1s, bind_executor(executor, as_tuple(deferred))));
        TVLOG(2) << "Closing socket";
        mSslStream->next_layer().close();
        mSslStream.reset();
    }
    if (mTcpSocket)
    {
        mTcpSocket->close();
    }
}

const std::string& CoHttpsClient::hostname() const
{
    return mHostname;
}

uint16_t CoHttpsClient::port() const
{
    return mPort;
}

bool CoHttpsClient::connected() const
{
    return mSslStream && mSslStream->next_layer().is_open();
}

const boost::asio::ip::tcp::endpoint& CoHttpsClient::currentEndpoint() const
{
    return mCurrentEndpoint;
}

CoHttpsClient::Strand& CoHttpsClient::strand() const
{
    Expect3(mStrand != nullptr, "stand must not be nullptr.", std::logic_error);
    return *mStrand;
}

void CoHttpsClient::setMandatoryFields(const std::string& xRequestId, Request& request) const
{

    if (! request.has_content_length())
    {
        request.set(Header::ContentLength, std::to_string(request.body().size()));
    }
    if (request.find(Header::Tee3::XUserAgent) == request.cend())
    {
        std::ostringstream userAgent;
        userAgent << "ERP-FD/" << ErpServerInfo::ReleaseVersion();
        request.set(Header::Tee3::XUserAgent, userAgent.view());
    }
    if (request.find(Header::Host) == request.cend())
    {
        request.set(Header::Host, hostname());
    }
    request.set(Header::XRequestId, xRequestId);
}
