/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_CLIENT_URLREQUESTSENDER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_CLIENT_URLREQUESTSENDER_HXX

#include "shared/network/client/ConnectionParameters.hxx"
#include "shared/network/client/ProxyParameters.hxx"
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
    explicit UrlRequestSender(TlsCertificateVerifier certificateVerifier, std::chrono::milliseconds connectionTimeout,
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
        const bool trustCn = false,
        int redirectDepth = 0) const;
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

    void setTlsCertificateVerifier(TlsCertificateVerifier certificateVerifier);

    /**
     * Set the list of proxies, for http connections, only proxies with
     * ProxyMode::HTTP are considered, while for HTTPS connections only proxies
     * in ProxyMode::SNI are used. Note that when a proxy is used, specifying the
     * endpoint (i.e. the IP) directly will be ignored.
     * Proxies are tried in the order they are passed, if a networking issue
     * is detected, the next proxy is tested.
     */
    void setProxies(std::vector<ProxyParameters> proxies);

    void setFollowRedirects(bool followRedirects);

    void setAdditionalHeaders(Header::keyValueMap_t&& additionalHeaders);

protected:
    virtual ClientResponse doSend(const std::string& url, HttpMethod method, const std::string& body,
                                  const std::string& contentType = std::string(),
                                  const std::optional<std::string>& forcedCiphers = std::nullopt, bool trustCn = false,
                                  const boost::asio::ip::tcp::endpoint* ep = nullptr) const;
    virtual ClientResponse doSend(const UrlHelper::UrlParts& url, HttpMethod method, const std::string& body,
                                  const std::string& contentType = std::string(),
                                  const std::optional<std::string>& forcedCiphers = std::nullopt, bool trustCn = false,
                                  const boost::asio::ip::tcp::endpoint* ep = nullptr) const;

    bool followsRedirects() const;

private:
    TlsCertificateVerifier mTlsCertificateVerifier;
    std::chrono::milliseconds mConnectionTimeout;
    std::chrono::milliseconds mResolveTimeout;
    mutable DurationConsumer mDurationConsumer;
    std::vector<ProxyParameters> mProxies;
    bool mFollowRedirects;
    Header::keyValueMap_t mAdditionalHeaders;
};


#endif
