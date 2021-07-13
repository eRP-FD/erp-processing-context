#include "test/mock/UrlRequestSenderMock.hxx"

#include "erp/util/Expect.hxx"


UrlRequestSenderMock::UrlRequestSenderMock(std::unordered_map<std::string, std::string> responses)
    : UrlRequestSender({}, 30/*httpClientConnectTimeoutSeconds*/)
    , mResponses(std::move(responses))
{
}

ClientResponse UrlRequestSenderMock::doSend(
    const std::string& url,
    const HttpMethod,
    const std::string& body,
    const std::string&,
    const std::optional<std::string>&,
    const bool) const
{
    std::lock_guard lock(mMutex);

    const auto functionIterator = mResponseGenerators.find(url);
    if (functionIterator != mResponseGenerators.end())
    {
        return functionIterator->second(body);
    }
    else
    {
        const auto iterator = mResponses.find(url);
        Expect(iterator != mResponses.end(), "Unexpected request to " + url);

        Header header;
        header.setStatus(HttpStatus::OK);
        header.setContentLength(iterator->second.size());
        ClientResponse response(header, iterator->second);

        return response;
    }
}

ClientResponse UrlRequestSenderMock::doSend(
    const UrlHelper::UrlParts& url,
    const HttpMethod method,
    const std::string& body,
    const std::string& contentType,
    const std::optional<std::string>& forcedCiphers,
    const bool trustCn) const
{
    return doSend(url.toString(), method, body, contentType, forcedCiphers, trustCn);
}

void UrlRequestSenderMock::setUrlHandler(
    const std::string& url, std::function<ClientResponse(const std::string&)> responseGenerator)
{
    std::lock_guard lock(mMutex);

    mResponseGenerators[url] = responseGenerator;
    mResponses.erase(url);
}
