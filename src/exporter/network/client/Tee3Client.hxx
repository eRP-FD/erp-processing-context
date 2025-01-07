/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef MEDICATION_EXPORTER_TEE3CLIENT_HXX
#define MEDICATION_EXPORTER_TEE3CLIENT_HXX

#include "fhirtools/util/Gsl.hxx"
#include "shared/network/client/CoHttpsClient.hxx"

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <chrono>

namespace epa
{
class BinaryBuffer;
class ClientTeeHandshake;
struct Tee3Context;
}

class Certificate;
class EpaCertificateService;
class HsmPool;
class MimeType;
class Tee3ClientsForHost;
class TslManager;

class Tee3Client
{
public:
    using Request = CoHttpsClient::Request;
    using Response = CoHttpsClient::Response;
    using Resolver = CoHttpsClient::Resolver;
    static constexpr std::chrono::steady_clock::duration retryTimeout = std::chrono::minutes{1};
    static constexpr std::chrono::steady_clock::duration maxRetryTimeout = std::chrono::minutes{10};
    static constexpr auto teeContextTimeout = std::chrono::hours{24};
    // additional error codes that can occur in TEE 3 handshake or during sendInner
    // other boost::asio error codes related to tcp and ssl can also occur
    enum class error
    {
        tee3_handshake_failed,
        outer_response_status_not_ok,
        extra_bytes_in_decrypted_inner_response,
        restart,
    };

    Tee3Client(boost::asio::io_context& ioContext, gsl::not_null<Tee3ClientsForHost*> owner);

    ~Tee3Client() noexcept;

    [[nodiscard]] boost::asio::awaitable<void> ensureConnected();

    [[nodiscard]] boost::asio::awaitable<boost::system::result<Response>> sendInner(Request request, std::unordered_map<std::string, std::any>& logDataBag);


    static const boost::system::error_category& errorCategory();

    [[nodiscard]] const boost::asio::ip::tcp::endpoint& currentEndpoint() const;

    [[nodiscard]] boost::asio::awaitable<void> close();

    Tee3ClientsForHost* owningClientsForHost();

    struct EndpointData {
        Resolver::endpoint_type endpoint;
        std::size_t retryCount;
        std::chrono::steady_clock::time_point nextRetry;
    };


private:
    boost::asio::awaitable<boost::system::error_code> tee3Handshake();

    bool needHandshake();

    [[nodiscard]] boost::asio::awaitable<boost::system::error_code> authorize(HsmPool& hsmPool);

    /// perform a tee3 handshake followed by authorization calls
    boost::asio::awaitable<boost::system::error_code> handshakeWithAuthorization();

    // try to connect to one of endpoints associated to the hostname
    /// @returns VAU-NP on success
    boost::asio::awaitable<boost::system::error_code> tryConnectLoop();
    // try to connect the tee3 client to a single endpoint
    /// @returns VAU-NP on success
    boost::asio::awaitable<boost::system::error_code> tryConnect(EndpointData endpointData);


    class Tee3ErrorCategory;

    boost::asio::awaitable<std::string> getFreshness();
    boost::asio::awaitable<std::string> sendAuthorizationRequest(HsmPool& hsmPool, std::string freshness);

    static Request createOuterRequest(boost::beast::http::verb verb, std::string_view target, const MimeType& mimeType,
                                      std::string_view userAgent);

    Certificate provideCertificate(const epa::BinaryBuffer& hash, uint64_t version);
    class TimeoutHelper;

    Tee3ClientsForHost* mOwingClientsForHost;
    CoHttpsClient mHttpsClient;
    // strand to serialize the access to the tee context
    boost::asio::strand<boost::asio::any_io_executor> mStrand;
    std::shared_ptr<epa::Tee3Context> mTee3Context;
    boost::asio::steady_timer mTeeContextRefreshTimer;
    EpaCertificateService& mCertificateService;
    std::string mDefaultUserAgent;
};

namespace boost::system
{
template<>
struct is_error_code_enum<Tee3Client::error> {
    static const bool value = true;
};
}

inline boost::system::error_code make_error_code(Tee3Client::error e)
{
    return boost::system::error_code(static_cast<int>(e), Tee3Client::errorCategory());
}


#endif// MEDICATION_EXPORTER_TEE3CLIENT_HXX
