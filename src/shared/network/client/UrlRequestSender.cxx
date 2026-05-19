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

#include <fmt/format.h>
#include <ranges>
#include <set>


UrlRequestSender::UrlRequestSender(std::string rootCertificates, const uint16_t connectionTimeoutSeconds,
                                   std::chrono::milliseconds resolveTimeout)
    : mTlsCertificateVerifier{TlsCertificateVerifier::withCustomRootCertificates(std::move(rootCertificates))}
    , mConnectionTimeout(std::chrono::seconds{connectionTimeoutSeconds})
    , mResolveTimeout{resolveTimeout}
    , mDurationConsumer()
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
    , mFollowRedirects(false)
{
}


void UrlRequestSender::setTlsCertificateVerifier(TlsCertificateVerifier certificateVerifier)
{
    mTlsCertificateVerifier = std::move(certificateVerifier);
}


void UrlRequestSender::setFollowRedirects(bool followRedirects)
{
    mFollowRedirects = followRedirects;
}


void UrlRequestSender::setAdditionalHeaders(Header::keyValueMap_t&& additionalHeaders)
{
    mAdditionalHeaders = std::move(additionalHeaders);
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

ClientResponse UrlRequestSender::doSend(const std::string& url, const HttpMethod method, const std::string& body,
                                        const std::string& contentType, const std::optional<std::string>& forcedCiphers,
                                        const bool trustCn, const boost::asio::ip::tcp::endpoint* ep) const
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
    Expect(url.isHttpsProtocol() || url.isHttpProtocol(), "unsupported protocol");

    const auto isTls = url.isHttpsProtocol();
    const auto proxyMode = isTls ? ProxyMode::SNI : ProxyMode::HTTP;
    auto byProxyMode = [proxyMode](const ProxyParameters& proxy) {
        return proxyMode == proxy.mode;
    };
    auto proxies = mProxies | std::views::filter(byProxyMode);
    const auto tlsParameters =
        isTls ? std::make_optional<TlsConnectionParameters>({.certificateVerifier = mTlsCertificateVerifier,
                                                             .forcedCiphers = forcedCiphers,
                                                             .trustCertificateCn = trustCn})
              : std::nullopt;
    const auto connectionParameters = ConnectionParameters{.hostname = url.mHost,
                                                           .port = std::to_string(url.mPort),
                                                           .connectionTimeout = mConnectionTimeout,
                                                           .resolveTimeout = mResolveTimeout,
                                                           .tlsParameters = tlsParameters};
    Header::keyValueMap_t httpHeaders;
    if (! body.empty())
    {
        httpHeaders.emplace(Header::ContentLength, std::to_string(body.size()));
    }
    if (! contentType.empty())
    {
        httpHeaders.emplace(Header::ContentType, contentType);
    }
    httpHeaders.emplace(Header::UserAgent, "erp-processing-context");
    httpHeaders.emplace(Header::Accept, "*/*");
    httpHeaders.emplace(Header::Host, fmt::format("{}:{}", url.mHost, url.mPort));
    httpHeaders.emplace(Header::Connection, Header::ConnectionClose);

    httpHeaders.insert(
        std::make_move_iterator(mAdditionalHeaders.begin()),
        std::make_move_iterator(mAdditionalHeaders.end())
    );

    const ClientRequest request(Header(method, std::string(url.mPath + url.mRest), Header::Version_1_1,
                                       std::move(httpHeaders), HttpStatus::Unknown),
                                body);

    auto sendHttpReq = [&request, &connectionParameters]<typename Client>(const boost::asio::ip::tcp::endpoint* ep) {
        if (ep != nullptr)
        {
            return Client(*ep, connectionParameters).send(request);
        }
        return Client(connectionParameters).send(request);
    };

    auto sendReq = [&sendHttpReq](bool isTls, const boost::asio::ip::tcp::endpoint* ep) {
        if (isTls)
        {
            return sendHttpReq.template operator()<HttpsClient>(ep);
        }
        else
        {
            return sendHttpReq.template operator()<HttpClient>(ep);
        }
    };

    // if there are no applicable proxies, directly connect to the host (or provided endpoint)
    if (proxies.empty())
    {
        return sendReq(isTls, ep);
    }
    // we have a proxy match, allow failure for individual connections, as we can have multiple connections
    Expect(ep == nullptr, "Cannot combine proxy and user-defined endpoint");
    for (const auto& proxy : proxies)
    {
        TVLOG(1)  << "trying proxy at " << proxy.ip << ":" << proxy.port;
        const auto proxyEndpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(proxy.ip), proxy.port);
        try
        {
            return sendReq(isTls, &proxyEndpoint);
        }
        catch (const boost::beast::system_error& e)
        {
            // in case of of networking errors, try the next proxy
            auto ec = e.code();
            if (boost::asio::ssl::error::stream_truncated == ec || boost::asio::error::connection_reset == ec ||
                boost::asio::error::not_connected == ec || boost::beast::http::error::end_of_stream == ec ||
                boost::asio::error::connection_refused == ec || boost::asio::error::host_unreachable == ec)
            {
                LOG(INFO) << "request to proxy " << proxy.ip << ":" << proxy.port << " failed with " << ec.message();
                continue;
            }
            throw;
        }
    }
    Fail("No proxy available to take request");
}

bool UrlRequestSender::followsRedirects() const
{
    return mFollowRedirects;
}


void UrlRequestSender::setProxies(std::vector<ProxyParameters> proxies)
{
    mProxies = std::move(proxies);
}
