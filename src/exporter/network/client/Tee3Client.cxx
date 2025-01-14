/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/network/client/Tee3Client.hxx"
#include "exporter/BdeMessage.hxx"
#include "exporter/network/client/Tee3ClientsForHost.hxx"
#include "exporter/tee3/EpaCertificateService.hxx"
#include "shared/erp-serverinfo.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/model/ProfessionOid.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/tsl/TslManager.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Demangle.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/HeaderLog.hxx"
#include "shared/util/Random.hxx"
#include "tee3/library/crypto/tee3/ClientTeeHandshake.hxx"
#include "tee3/library/crypto/tee3/Tee3Context.hxx"
#include "tee3/library/crypto/tee3/TeeMessageRestart.hxx"
#include "tee3/library/crypto/tee3/TeeStreamFactory.hxx"
#include "tee3/library/stream/StreamFactory.hxx"
#include "tee3/library/util/cbor/CborDeserializer.hxx"

#include <boost/asio/deferred.hpp>
#include <boost/asio/immediate.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/write.hpp>
#include <exporter/VauAutTokenSigner.hxx>

namespace
{
struct CborMessage
{
    std::string MessageType{};
    int ErrorCode{};
    std::string ErrorMessage{};

    template<typename Processor>
    constexpr void processMembers(Processor& processor)
    {
        processor("/MessageType", MessageType);
        processor("/ErrorCode", ErrorCode);
        processor("/ErrorMessage", ErrorMessage);
    }
};

std::optional<std::string> logCbor(const std::string& body)
{
    try
    {
        auto buf = epa::BinaryBuffer(body);
        const CborMessage m = epa::CborDeserializer::deserialize<CborMessage>(buf);
        return m.ErrorMessage;
    }
    catch (const epa::CborError& error)
    {
        TLOG(ERROR) << "Couldn't decode cbor message, cbor error.";
    }
    catch (const std::runtime_error& error)
    {
        TLOG(ERROR) << "Couldn't decode cbor message, runtime error.";
    }
    return std::nullopt;
}

std::optional<std::string> logJson(const std::string& bodyText)
{
    static const std::string ErrorMessageKey{"ErrorMessage"};
    static const std::string ErrorCodeKey{"errorCode"};
    rapidjson::Document document;
    document.Parse(bodyText.c_str());
    const auto fn = [] (rapidjson::Document& document, const std::string& key)-> std::optional<std::string>
    {
        if (document.HasMember(key) && document[key].IsString())
        {
            return std::string(document[key].GetString(), document[ErrorMessageKey].GetStringLength());
        };
        return std::nullopt;
    };
    if (!document.HasParseError())
    {
        return fn(document, ErrorMessageKey).value_or( fn(document, ErrorCodeKey).value_or("unknown") );
    }
    return std::nullopt;
}

void logDetails(
    const boost::system::result<boost::beast::http::message<false, boost::beast::http::basic_string_body<char>>>& outerResponse)
{
    const auto contentType = outerResponse->base().at(Header::ContentType);
    std::optional<std::string> errorMessage{};
    if (contentType == MimeType::cbor)
    {
        errorMessage = logCbor(outerResponse->body());
    }
    else if (contentType == MimeType::json)
    {
        errorMessage = logJson(outerResponse->body());
    }
    else
    {
        TLOG(ERROR) << "Unexpected content type in outer response.";
        return;
    }
    if (errorMessage.has_value())
    {
        BDEMessage bde;
        bde.mError = *errorMessage;
        TLOG(ERROR) << bde.mError;
    }
}

template<typename EndpointIteratorType>
void increaseEndpointBackoffTime(EndpointIteratorType endpoint, std::string_view hostname)
{
    endpoint->retryCount++;
    auto jitter = std::chrono::seconds{Random::randomIntBetween(0, 10)};
    auto backoffDuration = std::min(static_cast<long int>(std::pow(2, endpoint->retryCount)) * Tee3Client::retryTimeout,
                                    Tee3Client::maxRetryTimeout) +
                           jitter;

    if (backoffDuration >= Tee3Client::maxRetryTimeout)
    {
        TLOG(WARNING) << "ePA down on " << hostname << ":" << endpoint->endpoint.port() << " ("
                      << endpoint->endpoint.address() << "), backing off for "
                      << backoffDuration << ", retryCount = " << endpoint->retryCount;
    }

    endpoint->nextRetry = std::chrono::steady_clock::now() + backoffDuration;
}

template<bool isRequest>
std::string to_string(const CoHttpsClient::Message<isRequest>& msg)
{
    std::ostringstream oss;
    oss << msg;
    return std::move(oss).str();
}

boost::system::result<CoHttpsClient::Response> responseFromString(const std::string& data)
{
    boost::beast::http::parser<false, CoHttpsClient::Body> p;
    p.eager(true);
    boost::system::error_code ec;
    size_t count = p.put(boost::asio::buffer(data), ec);
    if (count != data.size())
    {
        return Tee3Client::error::extra_bytes_in_decrypted_inner_response;
    }
    return p.get();
}

TlsCertificateVerifier epaTlsCertificateVerifier(TslManager& tslManager)
{
    const auto& config = Configuration::instance();
    auto enforceServerAuth =
        config.getBoolValue(ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_ENFORCE_SERVER_AUTHENTICATION);
    return enforceServerAuth ? TlsCertificateVerifier::withTslVerification(
                                   tslManager, {.certificateType = CertificateType::C_FD_TLS_S,
                                                .ocspCheckMode = OcspCheckDescriptor::PROVIDED_OR_CACHE_REQUEST_IF_OUTDATED,
                                                .ocspGracePeriod = std::chrono::seconds(config.getIntValue(
                                                    ConfigurationKey::MEDICATION_EXPORTER_OCSP_EPA_GRACE_PERIOD)),
                                                .withSubjectAlternativeAddressValidation = true})
                                   .withAdditionalCertificateCheck([](const X509Certificate& cert) -> bool {
                                       return cert.checkRoles({std::string{profession_oid::oid_epa_dvw}});
                                   })
                             : TlsCertificateVerifier::withVerificationDisabledForTesting();
}

} // namespace

//NOLINTNEXTLINE(performance-unnecessary-value-param) owner is a pointer only - const reference to pointer is useless
Tee3Client::Tee3Client(boost::asio::io_context& ioContext, gsl::not_null<Tee3ClientsForHost*> owner)
    : mOwingClientsForHost{owner}
    , mHttpsClient{ioContext,
                   ConnectionParameters{
                       .hostname = owner->hostname(),
                       .port = std::to_string(owner->port()),
                       .connectionTimeout = std::chrono::milliseconds{Configuration::instance().getIntValue(
                           ConfigurationKey::MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_CONNECT_TIMEOUT_MILLISECONDS)},
                       .resolveTimeout = std::chrono::milliseconds{Configuration::instance().getIntValue(
                           ConfigurationKey::MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_RESOLVE_TIMEOUT_MILLISECONDS)},
                       .tlsParameters = TlsConnectionParameters{.certificateVerifier =
                                                                    epaTlsCertificateVerifier(owner->tslManager())}}}
    , mStrand{make_strand(ioContext)}
    , mTeeContextRefreshTimer{mStrand}
    , mCertificateService{boost::asio::use_service<EpaCertificateService>(ioContext)}
    , mDefaultUserAgent{
          Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_USER_AGENT)}
{
    if (! mCertificateService.hasTlsCertificateVerifier())
    {
        mCertificateService.setTlsCertificateVerifier(epaTlsCertificateVerifier(owner->tslManager()));
    }
}


Tee3Client::~Tee3Client() noexcept = default;

boost::asio::awaitable<boost::system::error_code> Tee3Client::handshakeWithAuthorization()
{
    auto ec = co_await tee3Handshake();

    if (! ec)
    {
        ec = co_await authorize(mOwingClientsForHost->hsmPool());
    }

    co_return ec;
}


boost::asio::awaitable<void> Tee3Client::ensureConnected()
{
    // In case that only the tee3 data is expired,
    // we can just retry the handshake. If it fails, disconnect and do the full reconnect loop
    // below
    if (mHttpsClient.connected() && needHandshake())
    {
        auto ec = co_await handshakeWithAuthorization();
        if (ec)
        {
            closeTeeSession();
            co_await closeTlsSession();
        }
    }
    // If its not connected, select a random endpoint, connect via TLS and do a tee3 handshake.
    // In case a connection/handshake fails, retry by selecting a new endpoint
    if (! mHttpsClient.connected())
    {
        auto ec = co_await tryConnectLoop();
        Expect(! ec.failed(), "No endpoint available, " + mHttpsClient.hostname() + " is down.");
        co_return;
    }
}

boost::asio::awaitable<boost::system::error_code> Tee3Client::tryConnectLoop()
{
    auto availableEndpoints = mOwingClientsForHost->endpoints();
    Expect(! availableEndpoints.empty(),
           "no endpoints for " + mHttpsClient.hostname() + ":" + std::to_string(mHttpsClient.port()));
    auto randomEndpoint = Random::selectRandomElement(availableEndpoints.begin(), availableEndpoints.end());
    availableEndpoints.erase(randomEndpoint);
    auto result = co_await tryConnect(*randomEndpoint);
    while (result.failed() && ! availableEndpoints.empty())
    {
        // resource_unavailable_try_again means the endpoint is currently in
        // backoff time, only increase the backoff time when there are other
        // errors
        if (result != boost::system::errc::resource_unavailable_try_again)
        {
            increaseEndpointBackoffTime(randomEndpoint, mHttpsClient.hostname());
        }
        randomEndpoint = Random::selectRandomElement(availableEndpoints.begin(), availableEndpoints.end());
        result = co_await tryConnect(*randomEndpoint);
        availableEndpoints.erase(randomEndpoint);
    }

    if (result.failed() && availableEndpoints.empty())
    {
        // we have tried all endpoints
        co_return boost::system::errc::make_error_code(boost::system::errc::resource_unavailable_try_again);
    }
    if (! result.failed())
    {
        randomEndpoint->retryCount = 0;
    }
    co_return result;
}


boost::asio::awaitable<boost::system::error_code> Tee3Client::tryConnect(EndpointData endpointData)
{
    if (endpointData.nextRetry > std::chrono::steady_clock::now())
    {
        co_return boost::system::errc::make_error_code(boost::system::errc::resource_unavailable_try_again);
    }
    HeaderLog::info([&] {
        return std::ostringstream{} << "Trying connect to " << mHttpsClient.hostname() << " on "
                                    << endpointData.endpoint.address();
    });

    auto ec = co_await mHttpsClient.connectToEndpoint(endpointData.endpoint);
    if (! ec && needHandshake())
    {
        ec = co_await handshakeWithAuthorization();
    }

    co_return ec;
}

const boost::asio::ip::tcp::endpoint& Tee3Client::currentEndpoint() const
{
    return mHttpsClient.currentEndpoint();
}

boost::asio::awaitable<void> Tee3Client::closeTlsSession()
{
    auto executor = co_await boost::asio::this_coro::executor;
    co_await boost::asio::dispatch(boost::asio::bind_executor(mStrand, boost::asio::deferred));
    co_await mHttpsClient.close();
    co_await boost::asio::dispatch(boost::asio::bind_executor(executor, boost::asio::deferred));
}

void Tee3Client::closeTeeSession()
{
    mTee3Context.reset();
    mTeeContextRefreshTimer.cancel();
}

Tee3ClientsForHost* Tee3Client::owningClientsForHost()
{
    return mOwingClientsForHost;
}

Tee3Client::Request Tee3Client::createOuterRequest(boost::beast::http::verb verb, std::string_view target,
                                                   const MimeType& mimeType, std::string_view userAgent)
{
    Request result{verb, target, 11};
    result.keep_alive(true);
    result.set(Header::Connection, Header::ConnectionKeepAlive);
    result.insert(Header::ContentType, std::string{mimeType});
    result.set(Header::Tee3::XUserAgent, userAgent);
    return result;
}

boost::asio::awaitable<boost::system::result<Tee3Client::Response>> Tee3Client::sendInner(Request request, std::unordered_map<std::string, std::any>& logDataBag)
{
    auto executor = co_await boost::asio::this_coro::executor;
    co_await boost::asio::dispatch(boost::asio::bind_executor(mStrand, boost::asio::deferred));
    using namespace std::string_literals;
    Expect3(mTee3Context != nullptr, "sendInner called but no tee 3 context available.", std::logic_error);
    using boost::beast::http::verb;
    mHttpsClient.setMandatoryFields(request);
    std::ostringstream serialized;
    serialized << request;
    HeaderLog::vlog(2, [&] {
        return std::ostringstream{} << "sending inner request: " << serialized.view();
    });
    auto contexts = mTee3Context->createSessionContexts();

    BDEMessage bde;
    bde.mStartTime = model::Timestamp::now();
    bde.mInnerOperation = request.target();
    bde.mCid = mTee3Context->vauCid.toString();
    bde.mHost = mHttpsClient.hostname();
    bde.mIp = mHttpsClient.currentEndpoint().address().to_string();
    if (request.base().find(Header::XRequestId) != request.base().end()) {
        bde.mRequestId = std::string{ request[Header::XRequestId] };
    }
    BDEMessage::assignIfContains(logDataBag, BDEMessage::prescriptionIdKey, bde.mPrescriptionId);
    BDEMessage::assignIfContains(logDataBag, BDEMessage::lastModifiedTimestampKey, bde.mLastModified);
    auto encrypted = epa::TeeStreamFactory::createReadableEncryptingStream(
        [&contexts]() {
            return contexts.request;
        },
        epa::StreamFactory::makeReadableStream(std::move(serialized).str()));
    Request outerRequest = createOuterRequest(verb::post, mTee3Context->vauCid.toString(), MimeType::binary,
                                              request[Header::Tee3::XUserAgent]);
    outerRequest.body() = encrypted.readAll().toString();
    co_await boost::asio::dispatch(boost::asio::bind_executor(executor, boost::asio::deferred));
    HeaderLog::vlog(3, [&] {
        return std::ostringstream{} << "sending outer request: " << Base64::encode(to_string(outerRequest));
    });
    auto outerResponse = co_await mHttpsClient.send(outerRequest);
    auto cleanup = gsl::finally([&bde, &outerResponse] {
        if (outerResponse.has_value()) {
            bde.mResponseCode = outerResponse->result_int();
        }
        else
        {
            bde.mError = outerResponse.error().message();
        }
        bde.mEndTime = model::Timestamp::now();
    });
    if (outerResponse.has_error())
    {
        co_await closeTlsSession();
        co_return outerResponse;
    }
    HeaderLog::vlog(3, [&] {
        return std::ostringstream{} << "received outer response: " << Base64::encode(to_string(outerResponse.value()));
    });
    if (outerResponse->result() != boost::beast::http::status::ok)
    {
        closeTeeSession();
        co_await closeTlsSession();
        logDetails(outerResponse);
        co_return error::outer_response_status_not_ok;
    }
    if (outerResponse.value()[Header::ContentType] == ContentMimeType::cbor)
    {
        const auto message =
            epa::CborDeserializer::deserialize<epa::TeeMessageRestart>(epa::BinaryBuffer{outerResponse->body()});
        Expect(epa::KeyId::fromBinaryView(message.keyId) == contexts.response.keyId,
               "response KeyId doesn't match request.");
        if (message.messageType == epa::Tee3Protocol::messageRestartName)
        {
            closeTeeSession();
            co_return error::restart;
        }
    }
    auto decrypted = epa::TeeStreamFactory::createReadableDecryptingStream(
        [&contexts](const epa::KeyId& keyId) {
            Expect(keyId == contexts.response.keyId, "response KeyId doesn't match request.");
            return contexts.response;
        },
        epa::StreamFactory::makeReadableStream(outerResponse->body()));
    std::string innerResponseData = decrypted.readAll().toString();
    HeaderLog::vlog(2, [&] {
        return std::ostringstream{} << "received inner response: " << innerResponseData;
    });
    auto response = responseFromString(innerResponseData);
    if (response)
    {
        bde.mInnerResponseCode = response->result_int();
    }
    else
    {
        closeTeeSession();
    }
    co_return response;
}


boost::asio::awaitable<boost::system::error_code> Tee3Client::tee3Handshake()
{
    try
    {
        mTeeContextRefreshTimer.cancel();
        using boost::beast::http::verb;
        Request request = createOuterRequest(verb::post, "/VAU", MimeType::cbor, mDefaultUserAgent);
        bool isPU = Configuration::instance().getBoolValue(ConfigurationKey::MEDICATION_EXPORTER_IS_PRODUCTION);
        epa::ClientTeeHandshake handshake{std::bind_front(&Tee3Client::provideCertificate, this),
                                          mOwingClientsForHost->tslManager(), isPU};
        request.body() = handshake.createMessage1().toString();
        if (auto vauNP = mOwingClientsForHost->vauNP())
        {
            HeaderLog::vlog(1, [&] {
                return std::ostringstream{} << mHttpsClient.hostname() << ":" << mHttpsClient.port()
                                            << " Using VAU-NP: " << *vauNP;
            });
            request.set(Header::Tee3::VauNP, *vauNP);
        }
        else
        {
            HeaderLog::vlog(1, [&] {
                return std::ostringstream{} << mHttpsClient.hostname() << ":" << mHttpsClient.port()
                                            << " no VAU-NP available.";
            });
        }
        HeaderLog::vlog(3, [&] {
            return std::ostringstream{} << "Sending Message 1";
        });

        BDEMessage bde;
        bde.mStartTime = model::Timestamp::now();
        bde.mInnerOperation = request.target();
        bde.mHost = mHttpsClient.hostname();
        if (request.base().find(Header::XRequestId) != request.base().end()) {
            bde.mRequestId = std::string{ request[Header::XRequestId] };
        }
        const auto m2response = co_await mHttpsClient.send(request);
        bde.mEndTime = model::Timestamp::now();
        if (m2response.has_error())
        {
            co_await closeTlsSession();
            HeaderLog::info([&] {
                return std::ostringstream{} << "tee3 handshake failed in message 1: " << m2response.error().message()
                                            << ": connecting to " << mHttpsClient.hostname() << " on "
                                            << mHttpsClient.currentEndpoint();
            });
            bde.mError = "handshake";
            co_return error::tee3_handshake_failed;
        }
        bde.mResponseCode = m2response->result_int();
        bde.mInnerResponseCode = m2response->result_int();
        Expect(m2response->result() == boost::beast::http::status::ok,
               "Message 1 request returned status: " + std::to_string(static_cast<uintmax_t>(m2response->result())));
        auto vauCid = m2response->find(Header::Tee3::VauCid);
        if (vauCid == m2response->end())
        {
            HeaderLog::info([&] {
                return std::ostringstream{} << "missing header '" << Header::Tee3::VauCid
                                            << "' in response: connecting to " << mHttpsClient.hostname() << " on "
                                            << mHttpsClient.currentEndpoint();
            });
            bde.mError = "missing header";
            co_return error::tee3_handshake_failed;
        }
        else
        {
            bde.mCid = vauCid->value();
        }
        handshake.setVauCid(epa::Tee3Protocol::VauCid{vauCid->value()});
        auto target = handshake.vauCid().toString();
        request = createOuterRequest(verb::post, target, MimeType::cbor, mDefaultUserAgent);
        request.body() = handshake.createMessage3(epa::BinaryBuffer{m2response->body()}).toString();
        HeaderLog::vlog(3, [&] {
            return std::ostringstream{} << "Sending Message 3 to: " << target;
        });
        BDEMessage bde3;
        bde3.mStartTime = model::Timestamp::now();
        bde3.mInnerOperation = request.target();
        bde3.mHost = mHttpsClient.hostname();
        if (request.base().find(Header::XRequestId) != request.base().end()) {
            bde3.mRequestId = std::string{ request[Header::XRequestId] };
        }
        const auto m4response = co_await mHttpsClient.send(request);
        bde3.mEndTime = model::Timestamp::now();
        if (m4response.has_error())
        {
            co_await closeTlsSession();
            HeaderLog::info([&] {
                return std::ostringstream{} << "tee3 handshake failed in message 3: " << m4response.error().message()
                                            << ": connecting to " << mHttpsClient.hostname() << " on "
                                            << mHttpsClient.currentEndpoint();
            });
            bde.mError = "handshake (3)";
            co_return error::tee3_handshake_failed;
        }
        bde3.mResponseCode = m4response->result_int();
        bde3.mInnerResponseCode = m4response->result_int();
        Expect(m4response->result() == boost::beast::http::status::ok,
               "Message 3 request returned status: " + std::to_string(static_cast<uintmax_t>(m4response->result())));
        handshake.processMessage4(epa::BinaryBuffer{m4response->body()});
        HeaderLog::vlog(1, [&] {
            return std::ostringstream{} << "Tee 3 handshake complete";
        });
        mTee3Context = handshake.context();
        mTeeContextRefreshTimer.expires_after(teeContextTimeout);
        mTeeContextRefreshTimer.async_wait([this](const boost::system::error_code& ec) {
            if (! ec)
            {
                HeaderLog::info("tee3 context expired, forcing refresh");
                mTee3Context.reset();
            }
        });
    }
    catch (const std::exception&)
    {
        co_return error::tee3_handshake_failed;
    }
    co_return boost::system::error_code{};
}

bool Tee3Client::needHandshake()
{
    return mTee3Context == nullptr;
}

Certificate Tee3Client::provideCertificate(const epa::BinaryBuffer& hash, uint64_t version)
{
    return mCertificateService.provideCertificate(mHttpsClient.hostname(), mHttpsClient.port(),
                                                  {ByteHelper::toHex(hash.getString()), version});
}

boost::asio::awaitable<boost::system::error_code> Tee3Client::authorize(HsmPool& hsmPool)
{
    auto executor = co_await boost::asio::this_coro::executor;
    boost::system::error_code result{};
    try
    {
        auto freshness = co_await getFreshness();
        auto vauNP = co_await sendAuthorizationRequest(hsmPool, std::move(freshness));
        co_await async_immediate(mStrand);
        mOwingClientsForHost->vauNP(vauNP);
    }
    catch (const std::exception& e)
    {
        HeaderLog::warning([&] {
            return std::ostringstream{} << "Unable to perform authorization request: " << e.what();
        });
        result = boost::system::errc::make_error_code(boost::system::errc::protocol_error);
    }
    if (result.failed())
    {
        closeTeeSession();
    }
    co_await async_immediate(executor);
    co_return result;
}

boost::asio::awaitable<std::string> Tee3Client::getFreshness()
{
    using boost::beast::http::verb;
    std::unordered_map<std::string, std::any> empty{};
    auto response = co_await sendInner(Request(verb::get, "/epa/authz/v1/freshness", 11), empty);
    Expect(! response.has_error(), "TEE request to freshness failed");
    Expect3(response.value().result() == boost::beast::http::status::ok, "Response for freshness request not 200",
            std::runtime_error);
    auto body = response.value().body();
    rapidjson::Document jsonBody;
    jsonBody.Parse(body);
    Expect(! jsonBody.HasParseError(), "response to freshness is not json");
    const auto freshnessIt = jsonBody.FindMember("freshness");
    Expect(freshnessIt != jsonBody.MemberEnd() && freshnessIt->value.IsString(), "field type must be string");
    co_return std::string{freshnessIt->value.GetString()};
}


boost::asio::awaitable<std::string> Tee3Client::sendAuthorizationRequest(HsmPool& hsmPool, std::string freshness)
{
    using boost::beast::http::verb;
    VauAutTokenSigner signer;
    const auto vauAutToken = signer.signAuthorizationToken(hsmPool.acquire().session(), freshness);

    auto authorizationReq = Tee3Client::Request(verb::post, "/epa/authz/v1/send_authorization_request_bearertoken", 11);
    using namespace std::string_literals;
    authorizationReq.body() = R"({"bearerToken":")"s.append(vauAutToken).append(R"("})");
    authorizationReq.set(Header::ContentType, static_cast<std::string>(MimeType::json));
    std::unordered_map<std::string, std::any> empty{};
    auto response = co_await sendInner(authorizationReq, empty);
    Expect(! response.has_error(), "TEE request to send_authorization_request_bearertoken failed");
    Expect(response.value().result() == boost::beast::http::status::ok,
           "Response for authorization_request request not 200");
    auto bodyAuthReq = response.value().body();
    rapidjson::Document jsonBody;
    jsonBody.Parse(bodyAuthReq);
    Expect(! jsonBody.HasParseError(), "response to send_authorization_request_bearertoken is not json");
    const auto vauNpMember = jsonBody.FindMember("vau-np");
    Expect(vauNpMember != jsonBody.MemberEnd() && vauNpMember->value.IsString(), "field vau-np must be string");
    std::string vauNP{vauNpMember->value.GetString()};
    Expect(! vauNP.empty(), "received empty VAU-NP");
    co_return vauNP;
}


class Tee3Client::Tee3ErrorCategory final : public boost::system::error_category {
public:
    const char* name() const noexcept final
    {
        return "Tee3Client";
    }
    using error_category::message;
    std::string message(Tee3Client::error err) const
    {
        switch (err)
        {
            case error::tee3_handshake_failed:
                return "TEE3 handshake failed";
            case error::outer_response_status_not_ok:
                return "Outer response status is not '200 OK'";
            case error::extra_bytes_in_decrypted_inner_response:
                return "Extra bytes in decrypted inner response.";
            case error::restart:
                return "TEE Server requested restart";
        }
        return "invalid Tee3Client::error: " + std::to_string(static_cast<uintmax_t>(err));
    }
    std::string message(int ev) const final
    {
        return message(static_cast<Tee3Client::error>(ev));
    }
    constexpr virtual ~Tee3ErrorCategory() final;
};

constexpr Tee3Client::Tee3ErrorCategory::~Tee3ErrorCategory() = default;

const boost::system::error_category& Tee3Client::errorCategory()
{
    static constexpr Tee3ErrorCategory category;
    return category;
}
