/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */
#include "exporter/tee3/EpaCertificateService.hxx"
#include "shared/crypto/Certificate.hxx"
#include "shared/network/client/CoHttpsClient.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/HeaderLog.hxx"
#include "tee3/library/crypto/tee3/Tee3Protocol.hxx"
#include "tee3/library/util/cbor/CborDeserializer.hxx"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_future.hpp>
#include <future>

boost::asio::execution_context::id EpaCertificateService::id;


EpaCertificateService::EpaCertificateService(boost::asio::execution_context& context)
    : service{context}
    , mWorkIoContext{}
    , mWork{boost::asio::make_work_guard(mWorkIoContext)}
    , mWorkThread{[this] {
        mWorkIoContext.run();
    }}
{
    TLOG(INFO) << "EpaCertificateService initialized";
}

EpaCertificateService::~EpaCertificateService()
{
    mWork.reset();
    if (mWorkThread.joinable())
    {
        mWorkThread.join();
    }
}

void EpaCertificateService::setTlsCertificateVerifier(TlsCertificateVerifier tlsCertificateVerifier)
{
    mTlsCertificateVerifier = std::move(tlsCertificateVerifier);
}


bool EpaCertificateService::hasTlsCertificateVerifier()
{
    return mTlsCertificateVerifier.has_value();
}


void EpaCertificateService::shutdown()
{
}

Certificate EpaCertificateService::provideCertificate(const std::string& hostname, uint16_t port, CertId certId)
{
    HeaderLog::vlog(2, [&] {
        return std::ostringstream{} << "requested certificate: " << certId;
    });
    // Certificate is not copyable, therefore just get the X509 pointer and convert it back
    // to Certificate
    auto fut = co_spawn(mWorkIoContext, provideCertificateInternal(hostname, port, certId), boost::asio::use_future);
    return Certificate(fut.get());
}

boost::asio::awaitable<shared_X509> EpaCertificateService::provideCertificateInternal(std::string hostname,
                                                                                      uint16_t port, CertId certId)
{
    auto result = mCertificates.find(certId);
    if (result != mCertificates.end())
    {
        TLOG(INFO) << "using cached certificate: {" << certId << R"( "subject": ")"
                   << x509NametoString(result->second.getSubjectName()) << R"("})";
        co_return result->second.toX509();
    }
    Expect(mTlsCertificateVerifier.has_value(), "TlsCertificateVerifier has not been set");
    const auto& config = Configuration::instance();
    const auto params = ConnectionParameters{
        .hostname = hostname,
        .port = std::to_string(port),
        .connectionTimeout = std::chrono::milliseconds{config.getIntValue(
            ConfigurationKey::MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_CONNECT_TIMEOUT_MILLISECONDS)},
        .resolveTimeout = std::chrono::milliseconds{config.getIntValue(
            ConfigurationKey::MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_RESOLVE_TIMEOUT_MILLISECONDS)},
        .tlsParameters = TlsConnectionParameters{.certificateVerifier = mTlsCertificateVerifier.value()}};
    auto client = CoHttpsClient{mWorkIoContext, params};
    auto ec = co_await client.resolveAndConnect();
    Expect(! ec, "Connection failure while obtaining certificate");
    CoHttpsClient::Request req{boost::beast::http::verb::get,
                               "/CertData." + certId.hash + '-' + std::to_string(certId.version), 11};
    req.set(Header::Tee3::XUserAgent,
            config.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_USER_AGENT));
    auto resp = co_await client.send(req);
    Expect(! resp.has_error(), "certificate download failed: cert: " + certId.hash +
                                   " version: " + std::to_string(certId.version) + ": " + resp.error().message());
    HeaderLog::vlog(3, [&] {
        return std::ostringstream{} << "received response for cert: " << certId << Base64::encode(resp->body());
    });
    auto certDer =
        epa::CborDeserializer::deserialize<epa::Tee3Protocol::AutTeeCertificate>(epa::BinaryBuffer{resp->body()})
            .certificate.getString();
    auto teeCert = Certificate::fromBinaryDer(certDer);
    mCertificates.emplace(certId, teeCert);
    TLOG(INFO) << "caching certificate: {" << certId << R"( "subject": ")" << x509NametoString(teeCert.getSubjectName())
               << R"("})";
    co_return teeCert.toX509();
}

std::ostream& operator<<(std::ostream& out, const EpaCertificateService::CertId& certId)
{
    return out << R"("cert": ")" << certId.hash << R"(", "version": ")" << certId.version << '"';
}

std::string to_string(const EpaCertificateService::CertId& certId)
{
    std::ostringstream out;
    out << certId;
    return std::move(out).str();
}
