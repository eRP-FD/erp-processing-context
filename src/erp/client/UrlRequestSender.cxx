/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/client/UrlRequestSender.hxx"

#include "erp/client/request/ClientRequest.hxx"
#include "erp/client/HttpClient.hxx"
#include "erp/client/HttpsClient.hxx"
#include "erp/util/ExceptionHelper.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "erp/util/UrlHelper.hxx"


UrlRequestSender::UrlRequestSender(
    SafeString rootCertificates,
    const uint16_t connectionTimeoutSeconds,
    std::chrono::milliseconds resolveTimeout,
    const bool enforceServerAuthentication)
    : mTslRootCertificates(std::move(rootCertificates))
    , mConnectionTimeoutSeconds(connectionTimeoutSeconds)
    , mResolveTimeout{resolveTimeout}
    , mEnforceServerAuthentication(enforceServerAuthentication)
    , mDurationConsumer()
{
}


ClientResponse UrlRequestSender::send(
    const std::string& url,
    const HttpMethod method,
    const std::string& body,
    const std::string& contentType,
    const std::optional<std::string>& forcedCiphers,
    const bool trustCn) const
{
    // TODO: add non-sensitive part of the url to the description.
    auto timer = mDurationConsumer.getTimer(DurationConsumer::categoryUrlRequestSender, "sending request");

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
    auto timer = mDurationConsumer.getTimer(DurationConsumer::categoryUrlRequestSender,
                                            "sending request to host " + url.mHost + ":" + std::to_string(url.mPort));

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
    auto timer =
        mDurationConsumer.getTimer(DurationConsumer::categoryUrlRequestSender, std::string{"sending request to host "}
                                                                                   .append(url.mHost)
                                                                                   .append(std::to_string(ep.port()))
                                                                                   .append(" (")
                                                                                   .append(ep.address().to_string())
                                                                                   .append(")"));

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

    const std::string lowerProtocol = String::toLower(url.mProtocol);
    if (lowerProtocol == "https://")
    {
        if (ep != nullptr)
        {
            return HttpsClient(*ep, url.mHost, mConnectionTimeoutSeconds, mEnforceServerAuthentication,
                               mTslRootCertificates, SafeString(), SafeString(), forcedCiphers)
                .send(request, trustCn);
        }
        return HttpsClient(
                   url.mHost,
                   gsl::narrow_cast<uint16_t>(url.mPort),
                   mConnectionTimeoutSeconds,
                   mResolveTimeout,
                   mEnforceServerAuthentication,
                   mTslRootCertificates,
                   SafeString(),
                   SafeString(),
                   forcedCiphers).send(request, trustCn);
    }
    else if (lowerProtocol == "http://")
    {
        if (ep != nullptr)
        {
            return HttpClient(*ep, url.mHost, mConnectionTimeoutSeconds).send(request);
        }
        return HttpClient(url.mHost, gsl::narrow_cast<uint16_t>(url.mPort), mConnectionTimeoutSeconds, mResolveTimeout)
            .send(request);
    }
    else
    {
        Fail2("unsupported protocol", std::runtime_error);
    }
}
