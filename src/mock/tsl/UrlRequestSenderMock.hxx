/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_MOCK_URLREQUESTSENDERMOCK_HXX
#define ERP_PROCESSING_CONTEXT_TEST_MOCK_URLREQUESTSENDERMOCK_HXX

#include "erp/client/UrlRequestSender.hxx"
#include "erp/crypto/Certificate.hxx"
#include "mock/tsl/MockOcsp.hxx"

#include <mutex>

#if WITH_HSM_MOCK  != 1
#error UrlRequestSenderMock.hxx included but WITH_HSM_MOCK not enabled
#endif

class UrlRequestSenderMock : public UrlRequestSender
{
public:
    explicit UrlRequestSenderMock(std::unordered_map<std::string, std::string> responses);
    ~UrlRequestSenderMock() override = default;

    void setUrlHandler(const std::string& url, std::function<ClientResponse(const std::string&)> responseGenerator);
    void setResponse(const std::string& url, const std::string& response);


    void setOcspUrlRequestHandler(const std::string& url,
                                  const std::vector<MockOcsp::CertificatePair>& ocspResponderKnownCertificateCaPairs,
                                  const Certificate& defaultOcspCertificate, shared_EVP_PKEY defaultOcspPrivateKey);

protected:
    ClientResponse doSend (
        const std::string& url,
        const HttpMethod method,
        const std::string& body,
        const std::string& contentType = std::string(),
        const std::optional<std::string>& forcedCiphers = std::nullopt,
        const bool trustCn = false,
        const boost::asio::ip::tcp::endpoint* ep = nullptr) const override;
    ClientResponse doSend (
        const UrlHelper::UrlParts& url,
        const HttpMethod method,
        const std::string& body,
        const std::string& contentType = std::string(),
        const std::optional<std::string>& forcedCiphers = std::nullopt,
        const bool trustCn = false,
        const boost::asio::ip::tcp::endpoint* ep = nullptr) const override;

private:
    mutable std::mutex mMutex;

    std::unordered_map<std::string, std::string> mResponses;
    std::unordered_map<std::string, std::function<ClientResponse(const std::string&)>> mResponseGenerators;
};


#endif
