/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/client/push/PushGatewayClient.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "exporter/model/push/EncryptedNotification.hxx"
#include "shared/crypto/CertificateChainAndKey.hxx"
#include "shared/network/client/ConnectionParameters.hxx"
#include "shared/network/client/UrlRequestSender.hxx"
#include "shared/network/client/response/ClientResponse.hxx"
#include "shared/network/message/HttpMethod.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/FileHelper.hxx"

#include <fmt/format.h>
#include <magic_enum/magic_enum.hpp>

class PushGatewayClientImpl final : public PushGatewayClient
{
public:
    static std::unique_ptr<UrlRequestSender> createRequestSender(CrlProvider* crlProvider);
    PushGatewayClientImpl(std::shared_ptr<CrlProvider> crlProvider)
        : mCrlProvider{std::move(crlProvider)}
        , mRequestSender{createRequestSender(mCrlProvider.get())}
    {
    }
    ClientResponse send(const UrlHelper::UrlParts& url, HttpMethod method, const std::string& body,
                        const std::string& contentType) const override;
    ClientResponse sendPushNotification(const model::EncryptedNotification& encryptedNotification,
                                        const std::string& baseUrl) override;


    std::shared_ptr<CrlProvider> mCrlProvider;// holds ownership for RequestSender
    std::unique_ptr<UrlRequestSender> mRequestSender;
    ~PushGatewayClientImpl() override;
};

PushGatewayClient::~PushGatewayClient() = default;

std::unique_ptr<PushGatewayClient> PushGatewayClient::create(std::shared_ptr<CrlProvider> crlProvider)
{
    return std::make_unique<PushGatewayClientImpl>(std::move(crlProvider));
}

PushGatewayClientImpl::~PushGatewayClientImpl() = default;

std::unique_ptr<UrlRequestSender> PushGatewayClientImpl::createRequestSender(CrlProvider* crlProvider)
{
    const auto& config = Configuration::instance();
    using enum ConfigurationKey;
    // GEMREQ-start A_27855
    auto tlsVerifier = TlsCertificateVerifier::withInternetRootCAsWithFallback(MEDICATION_EXPORTER_PUSH_CLIENT_SERVER_CA);
    if (crlProvider)
    {
        tlsVerifier.withCrl(*crlProvider, TlsCertificateVerifier::CrlMode::HARD_FAIL);
    }
    std::chrono::seconds connectTimeout{config.getIntValue(HTTPCLIENT_CONNECT_TIMEOUT_SECONDS)};
    std::chrono::milliseconds resolveTimeout{config.getIntValue(HTTPCLIENT_RESOLVE_TIMEOUT_MILLISECONDS)};
    auto sender = std::make_unique<UrlRequestSender>(std::move(tlsVerifier), connectTimeout, resolveTimeout);
    // GEMREQ-end A_27855
    if (config.getBoolValue(MEDICATION_EXPORTER_PUSH_CLIENT_USE_PROXY))
    {
        sender->setProxies(config.proxyParameters(ProxyMode::SNI));
    }
    // GEMREQ-start ​A_28267
    if (const auto& tlsClientCertificate = config.getOptionalPemValue(MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE))
    {
        const auto& tlsKey = value(config.getOptionalPemValue(MEDICATION_EXPORTER_PUSH_CLIENT_KEY));
        sender->setClientCertificateChainAndKey(CertificateChainAndKey::fromPem(*tlsClientCertificate, tlsKey));
    }
    // GEMREQ-end ​A_28267
    else
    {
        TLOG(INFO) << "PushGatewayClient: no mTLS configured - disabled";
    }
    return sender;
}

ClientResponse PushGatewayClientImpl::send(const UrlHelper::UrlParts& url, const HttpMethod method,
                                           const std::string& body, const std::string& contentType) const
{
    return mRequestSender->send(url, method, body, contentType);
}

ClientResponse PushGatewayClientImpl::sendPushNotification(const model::EncryptedNotification& encryptedNotification,
                                                           const std::string& baseUrl)
{
    A_27163.start("den gespeicherten Endpunkt des Push Gateways unter Verwendung von [OpenApi_Notification_PushGateway] aufrufen");
    auto url = UrlHelper::parseUrl(baseUrl);
    url.appendPath("notifyEncrypted/batch");
    return send(url, HttpMethod::POST, model::serializePushNotifications({{Uuid{}.toString(), encryptedNotification}}),
                "application/json");
}
