/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_CLIENT_URLREQUESTSENDER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_CLIENT_URLREQUESTSENDER_HXX

#include "shared/network/client/ConnectionParameters.hxx"
#include "shared/network/client/response/ClientResponse.hxx"
#include "shared/util/DurationConsumer.hxx"
#include "shared/util/UrlHelper.hxx"

#include <boost/asio/ip/tcp.hpp>

class HttpClient;
class HttpsClient;

class UrlRequestSender
{
public:
    explicit UrlRequestSender(std::string rootCertificates, const uint16_t connectionTimeoutSeconds,
                              std::chrono::milliseconds resolveTimeout);
    virtual ~UrlRequestSender() = default;

    /**
     * Send the given body to the URL and return the received response.
     * Only http and https protocols are supported.
     */
    ClientResponse send (
        const std::string& url,
        const HttpMethod method,
        const std::string& body,
        const std::string& contentType = std::string(),
        const std::optional<std::string>& forcedCiphers = std::nullopt,
        const bool trustCn = false) const;
    ClientResponse send (
        const UrlHelper::UrlParts& url,
        const HttpMethod method,
        const std::string& body,
        const std::string& contentType = std::string(),
        const std::optional<std::string>& forcedCiphers = std::nullopt,
        const bool trustCn = false) const;

    ClientResponse send(const boost::asio::ip::tcp::endpoint& ep, const UrlHelper::UrlParts& url,
                        const HttpMethod method, const std::string& body,
                        const std::string& contentType = std::string(),
                        const std::optional<std::string>& forcedCiphers = std::nullopt,
                        const bool trustCn = false) const;

    void setDurationConsumer (DurationConsumer&& durationConsumer);

protected:
    virtual ClientResponse doSend (
        const std::string& url,
        const HttpMethod method,
        const std::string& body,
        const std::string& contentType = std::string(),
        const std::optional<std::string>& forcedCiphers = std::nullopt,
        const bool trustCn = false,
        const boost::asio::ip::tcp::endpoint* ep = nullptr) const;
    virtual ClientResponse doSend (
        const UrlHelper::UrlParts& url,
        const HttpMethod method,
        const std::string& body,
        const std::string& contentType = std::string(),
        const std::optional<std::string>& forcedCiphers = std::nullopt,
        const bool trustCn = false,
        const boost::asio::ip::tcp::endpoint* ep = nullptr) const;

private:
    TlsCertificateVerifier mTlsCertificateVerifier;
    const uint16_t mConnectionTimeoutSeconds;
    std::chrono::milliseconds mResolveTimeout;
    mutable DurationConsumer mDurationConsumer;
};


#endif
