/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "mock/tsl/UrlRequestSenderMock.hxx"

#include "shared/util/Expect.hxx"


UrlRequestSenderMock::UrlRequestSenderMock(std::unordered_map<std::string, std::string> responses)
    : UrlRequestSender({}, 30/*httpClientConnectTimeoutSeconds*/, std::chrono::milliseconds{2000})
    , mResponses(std::move(responses))
{
}

UrlRequestSenderMock::UrlRequestSenderMock(TlsCertificateVerifier certificateVerifier,
                                           std::chrono::milliseconds connectionTimeout,
                                           std::chrono::milliseconds resolveTimeout)
    : UrlRequestSender(std::move(certificateVerifier), connectionTimeout, resolveTimeout)
{}


void UrlRequestSenderMock::setStrict(bool strict)
{
    mStrict = strict;
}


ClientResponse UrlRequestSenderMock::doSend(
    const std::string& url,
    const HttpMethod method,
    const std::string& body,
    const std::string& contentType,
    const std::optional<std::string>& forcedCiphers,
    const bool trustCn,
    const boost::asio::ip::tcp::endpoint* ep) const
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
        const bool hasResponse = iterator != mResponses.end();
        if (! hasResponse)
        {
            if (mStrict)
            {
                Fail("Unexpected request to " + url);
            }

            LOG(INFO) << "Passing request to RequestSender for " << url;
            const UrlHelper::UrlParts parts = UrlHelper::parseUrl(url);
            return UrlRequestSender::doSend(parts, method, body, contentType, forcedCiphers, trustCn, ep);
        }

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
    const bool trustCn,
    const boost::asio::ip::tcp::endpoint* ep) const
{
    return doSend(url.toString(), method, body, contentType, forcedCiphers, trustCn, ep);
}

void UrlRequestSenderMock::setUrlHandler(
    const std::string& url, std::function<ClientResponse(const std::string&)> responseGenerator)
{
    std::lock_guard lock(mMutex);

    mResponseGenerators[url] = std::move(responseGenerator);
    mResponses.erase(url);
}

void UrlRequestSenderMock::setResponse(const std::string& url, const std::string& response)
{
    std::lock_guard lock(mMutex);
    mResponses[url] = response;
    mResponseGenerators.erase(url);
}


void UrlRequestSenderMock::setOcspUrlRequestHandler(
    const std::string& url, const std::vector<MockOcsp::CertificatePair>& ocspResponderKnownCertificateCaPairs,
    const Certificate& defaultOcspCertificate, shared_EVP_PKEY defaultOcspPrivateKey)
{
    setUrlHandler(url,
                  [ocspResponderKnownCertificateCaPairs, ocspCertificate = defaultOcspCertificate,
                   ocspPrivateKey = std::move(defaultOcspPrivateKey)](const std::string& request) mutable -> ClientResponse {
                      const std::string responseBody = MockOcsp::create(request, ocspResponderKnownCertificateCaPairs,
                                                                        ocspCertificate, ocspPrivateKey)
                                                           .toDer();

                      Header header;
                      header.setStatus(HttpStatus::OK);
                      header.setContentLength(responseBody.size());
                      return ClientResponse(header, responseBody);
                  });
}
