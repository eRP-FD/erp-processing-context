/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/UrlRequestSender.hxx"
#include "shared/network/client/request/ClientRequest.hxx"
#include "shared/network/client/HttpClient.hxx"
#include "shared/network/client/HttpsClient.hxx"
#include "shared/util/ExceptionHelper.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/UrlHelper.hxx"


UrlRequestSender::UrlRequestSender(std::string rootCertificates, const uint16_t connectionTimeoutSeconds,
                                   std::chrono::milliseconds resolveTimeout)
    : mTlsCertificateVerifier{rootCertificates.empty()
                                  ? TlsCertificateVerifier::withBundledRootCertificates()
                                  : TlsCertificateVerifier::withCustomRootCertificates(std::move(rootCertificates))}
    , mConnectionTimeout(std::chrono::seconds{connectionTimeoutSeconds})
    , mResolveTimeout{resolveTimeout}
    , mDurationConsumer()
{
}

UrlRequestSender::UrlRequestSender(TlsCertificateVerifier certificateVerifier,
                                   std::chrono::milliseconds connectionTimeout,
                                   std::chrono::milliseconds resolveTimeout)
    : mTlsCertificateVerifier{std::move(certificateVerifier)}
    , mConnectionTimeout(connectionTimeout)
    , mResolveTimeout{resolveTimeout}
    , mDurationConsumer()
{
}


void UrlRequestSender::setTlsCertificateVerifier(TlsCertificateVerifier certificateVerifier)
{
    mTlsCertificateVerifier = std::move(certificateVerifier);
}


ClientResponse UrlRequestSender::send(
    const std::string& url,
    const HttpMethod method,
    const std::string& body,
    const std::string& contentType,
    const std::optional<std::string>& forcedCiphers,
    const bool trustCn) const
{
    const auto urlParts = UrlHelper::parseUrl(url);
    auto timer = mDurationConsumer.getTimer(DurationConsumer::categoryUrlRequestSender, urlParts.mHost);

    try
    {
        return doSend(url, method, body, contentType, forcedCiphers, trustCn);
    }
    catch(...)
    {
        ExceptionHelper::extractInformationAndRethrow(
            [&timer](std::string&& details, std::string&& location)
            {
                timer.notifyFailure(details + " at " + location);
            });
    }
}


ClientResponse UrlRequestSender::send(
    const UrlHelper::UrlParts& url,
    const HttpMethod method,
    const std::string& body,
    const std::string& contentType,
    const std::optional<std::string>& forcedCiphers,
    const bool trustCn) const
{
    auto timer = mDurationConsumer.getTimer(DurationConsumer::categoryUrlRequestSender, url.mHost);
    timer.keyValue("port", std::to_string(url.mPort));

    try
    {
        return doSend(url, method, body, contentType, forcedCiphers, trustCn);
    }
    catch(...)
    {
        ExceptionHelper::extractInformationAndRethrow(
            [&timer](std::string&& details, std::string&& location)
            {
            timer.notifyFailure(details + " at " + location);
            });
    }
}

ClientResponse UrlRequestSender::send(const boost::asio::ip::tcp::endpoint& ep, const UrlHelper::UrlParts& url,
                                      const HttpMethod method, const std::string& body, const std::string& contentType,
                                      const std::optional<std::string>& forcedCiphers, const bool trustCn) const
{
    auto timer = mDurationConsumer.getTimer(DurationConsumer::categoryUrlRequestSender, url.mHost);
    timer.keyValue("ip", ep.address().to_string());

    try
    {
        return doSend(url, method, body, contentType, forcedCiphers, trustCn, &ep);
    }
    catch (...)
    {
        ExceptionHelper::extractInformationAndRethrow([&timer](std::string&& details, std::string&& location) {
            timer.notifyFailure(details + " at " + location);
        });
    }
}

ClientResponse UrlRequestSender::doSend(
    const std::string& url,
    const HttpMethod method,
    const std::string& body,
    const std::string& contentType,
    const std::optional<std::string>& forcedCiphers,
    const bool trustCn,
    const boost::asio::ip::tcp::endpoint* ep) const
{
    const UrlHelper::UrlParts parts = UrlHelper::parseUrl(url);
    return doSend(parts, method, body, contentType, forcedCiphers, trustCn, ep);
}


ClientResponse UrlRequestSender::doSend(
    const UrlHelper::UrlParts& url,
    const HttpMethod method,
    const std::string& body,
    const std::string& contentType,
    const std::optional<std::string>& forcedCiphers,
    const bool trustCn,
    const boost::asio::ip::tcp::endpoint* ep) const
{
    TVLOG(1) << "request to Host [" << url.mHost << "], URL: " << url.toString();

    Header::keyValueMap_t httpHeaders;
    if ( ! body.empty())
    {
        httpHeaders.emplace(Header::ContentLength, std::to_string(body.size()));
    }
    if ( ! contentType.empty())
    {
        httpHeaders.emplace(Header::ContentType, contentType);
    }
    httpHeaders.emplace(Header::UserAgent, "erp-processing-context");
    httpHeaders.emplace(Header::Accept, "*/*");
    httpHeaders.emplace(Header::Host, url.mHost);
    httpHeaders.emplace(Header::Connection, Header::ConnectionClose);

    const ClientRequest request(
        Header(method, std::string(url.mPath + url.mRest),
               Header::Version_1_1, std::move(httpHeaders), HttpStatus::Unknown),
        body);

    if (url.isHttpsProtocol())
    {
        auto params = ConnectionParameters{
                .hostname =  url.mHost,
                .port = std::to_string(url.mPort),
                .connectionTimeout = mConnectionTimeout,
                .resolveTimeout = mResolveTimeout,
                .tlsParameters = TlsConnectionParameters{
                    .certificateVerifier = mTlsCertificateVerifier,
                    .forcedCiphers = forcedCiphers,
                    .trustCertificateCn = trustCn
                }
        };
        if (ep != nullptr)
        {
            return HttpsClient(*ep, params).send(request);
        }
        return HttpsClient(params).send(request);
    }
    else if (url.isHttpProtocol())
    {
        if (ep != nullptr)
        {
            return HttpClient(*ep, url.mHost, mConnectionTimeout).send(request);
        }
        return HttpClient(url.mHost, gsl::narrow_cast<uint16_t>(url.mPort), mConnectionTimeout, mResolveTimeout)
            .send(request);
    }
    else
    {
        Fail2("unsupported protocol", std::runtime_error);
    }
}
