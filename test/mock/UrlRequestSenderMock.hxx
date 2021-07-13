#ifndef ERP_PROCESSING_CONTEXT_TEST_MOCK_URLREQUESTSENDERMOCK_HXX
#define ERP_PROCESSING_CONTEXT_TEST_MOCK_URLREQUESTSENDERMOCK_HXX

#include "erp/client/UrlRequestSender.hxx"

#include <mutex>

class UrlRequestSenderMock : public UrlRequestSender
{
public:
    explicit UrlRequestSenderMock(std::unordered_map<std::string, std::string> responses);
    virtual ~UrlRequestSenderMock() = default;


    void setUrlHandler(const std::string& url, std::function<ClientResponse(const std::string&)> responseGenerator);

protected:
    virtual ClientResponse doSend (
        const std::string& url,
        const HttpMethod method,
        const std::string& body,
        const std::string& contentType = std::string(),
        const std::optional<std::string>& forcedCiphers = std::nullopt,
        const bool trustCn = false) const override;
    virtual ClientResponse doSend (
        const UrlHelper::UrlParts& url,
        const HttpMethod method,
        const std::string& body,
        const std::string& contentType = std::string(),
        const std::optional<std::string>& forcedCiphers = std::nullopt,
        const bool trustCn = false) const override;

private:
    mutable std::mutex mMutex;

    std::unordered_map<std::string, std::string> mResponses;
    std::unordered_map<std::string, std::function<ClientResponse(const std::string&)>> mResponseGenerators;
};


#endif
