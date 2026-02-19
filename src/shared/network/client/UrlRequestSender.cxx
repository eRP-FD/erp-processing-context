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

#include <set>


UrlRequestSender::UrlRequestSender(std::string rootCertificates, const uint16_t connectionTimeoutSeconds,
                                   std::chrono::milliseconds resolveTimeout)
    : mTlsCertificateVerifier{TlsCertificateVerifier::withCustomRootCertificates(std::move(rootCertificates))}
    , mConnectionTimeout(std::chrono::seconds{connectionTimeoutSeconds})
    , mResolveTimeout{resolveTimeout}
    , mDurationConsumer()
    , mProxyUrl(std::nullopt)
    , mFollowRedirects(false)
{
}

UrlRequestSender::UrlRequestSender(TlsCertificateVerifier certificateVerifier,
                                   std::chrono::milliseconds connectionTimeout,
                                   std::chrono::milliseconds resolveTimeout)
    : mTlsCertificateVerifier{std::move(certificateVerifier)}
    , mConnectionTimeout(connectionTimeout)
    , mResolveTimeout{resolveTimeout}
    , mDurationConsumer()
    , mProxyUrl(std::nullopt)
    , mFollowRedirects(false)
{
}


void UrlRequestSender::setTlsCertificateVerifier(TlsCertificateVerifier certificateVerifier)
{
    mTlsCertificateVerifier = std::move(certificateVerifier);
}


void UrlRequestSender::setProxyUrl(std::optional<std::string> url)
{
    mProxyUrl = std::move(url);
}


void UrlRequestSender::setFollowRedirects(bool followRedirects)
{
    mFollowRedirects = followRedirects;
}


ClientResponse UrlRequestSender::send(
    const std::string& url,
    const HttpMethod method,
    const std::string& body,
    const std::string& contentType,
    const std::optional<std::string>& forcedCiphers,
    const bool trustCn,
    int redirectDepth) const
{
    const std::set redirectStates{HttpStatus::MovedPermanently, HttpStatus::PermanentRedirect, HttpStatus::Found, HttpStatus::SeeOther, HttpStatus::TemporaryRedirect};
    constexpr int maxRedirects = 2;

    if (redirectDepth >= maxRedirects)
    {
        Fail("Too many redirects");
    }

    const auto urlParts = UrlHelper::parseUrl(url);
    auto timer = mDurationConsumer.getTimer(DurationCategory::httpclient, urlParts.mHost);

    try
    {
        auto response = doSend(url, method, body, contentType, forcedCiphers, trustCn);
        if (mFollowRedirects && redirectStates.contains(response.getHeader().status()))
        {
            auto location = response.getHeader().header("Location");
            if (location.has_value())
            {
                TVLOG(1) << "Following redirect to: " << *location;
                return send(*location, method, body, contentType, forcedCiphers, trustCn, redirectDepth + 1);
            }

            TLOG(WARNING) << "Missing Location header for redirect status: " << response.getHeader().status();
        }
        return response;
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
    auto timer = mDurationConsumer.getTimer(DurationCategory::httpclient, url.mHost);
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
    auto timer = mDurationConsumer.getTimer(DurationCategory::httpclient, url.mHost);
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
    const boost::asio::ip::tcp::endpoint* ep,
    const std::string& headerFieldHost) const
{
    if (mProxyUrl.has_value() && not mProxyUrl->empty())
    {
        const UrlHelper::UrlParts urlPartsOrig = UrlHelper::parseUrl(url);

        const std::string proxyUrl = *mProxyUrl + urlPartsOrig.mPath;
        const UrlHelper::UrlParts urlPartsProxy = UrlHelper::parseUrl(proxyUrl);

        const std::string targetUrl = urlPartsOrig.mHost + ":" + std::to_string(urlPartsOrig.mPort);
        TVLOG(1) << "Using proxy " << proxyUrl << " for access " << urlPartsOrig.mHost;
        return doSend(urlPartsProxy, method, body, contentType, forcedCiphers, trustCn, ep, targetUrl);
    }
    const UrlHelper::UrlParts parts = UrlHelper::parseUrl(url);
    return doSend(parts, method, body, contentType, forcedCiphers, trustCn, ep, headerFieldHost);
}


ClientResponse UrlRequestSender::doSend(
    const UrlHelper::UrlParts& url,
    const HttpMethod method,
    const std::string& body,
    const std::string& contentType,
    const std::optional<std::string>& forcedCiphers,
    const bool trustCn,
    const boost::asio::ip::tcp::endpoint* ep,
    const std::string& headerFieldHost) const
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
    if (headerFieldHost.empty())
    {
        httpHeaders.emplace(Header::Host, url.mHost);
    }
    else
    {
        httpHeaders.emplace(Header::Host, headerFieldHost);
    }
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

bool UrlRequestSender::followsRedirects() const
{
    return mFollowRedirects;
}
