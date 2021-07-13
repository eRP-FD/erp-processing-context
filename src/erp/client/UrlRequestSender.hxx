#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_CLIENT_URLREQUESTSENDER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_CLIENT_URLREQUESTSENDER_HXX

#include "erp/client/response/ClientResponse.hxx"
#include "erp/util/DurationConsumer.hxx"
#include "erp/util/SafeString.hxx"
#include "erp/util/UrlHelper.hxx"


class HttpClient;
class HttpsClient;

class UrlRequestSender
{
public:
    explicit UrlRequestSender(
        SafeString rootCertificates,
        const uint16_t connectionTimeoutSeconds,
        const bool enforceServerAuthentication = true);
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

    void setDurationConsumer (DurationConsumer&& durationConsumer);

protected:
    virtual ClientResponse doSend (
        const std::string& url,
        const HttpMethod method,
        const std::string& body,
        const std::string& contentType = std::string(),
        const std::optional<std::string>& forcedCiphers = std::nullopt,
        const bool trustCn = false) const;
    virtual ClientResponse doSend (
        const UrlHelper::UrlParts& url,
        const HttpMethod method,
        const std::string& body,
        const std::string& contentType = std::string(),
        const std::optional<std::string>& forcedCiphers = std::nullopt,
        const bool trustCn = false) const;

private:
    const SafeString mTslRootCertificates;
    const uint16_t mConnectionTimeoutSeconds;
    const bool mEnforceServerAuthentication;
    mutable DurationConsumer mDurationConsumer;
};


#endif
