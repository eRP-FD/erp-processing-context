/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/client/OAuthClientBase.hxx"
#include "exporter/BdeMessage.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "exporter/client/TokenCache.hxx"
#include "exporter/model/TaskEvent.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "shared/network/client/ClientInterface.hxx"
#include "shared/network/client/CrlProvider.hxx"
#include "shared/network/client/HttpsClient.hxx"
#include "shared/network/client/request/ClientRequest.hxx"
#include "shared/network/client/response/ClientResponse.hxx"
#include "shared/network/message/HttpStatus.hxx"
#include "shared/util/Configuration.hxx"

#include <boost/beast/core/error.hpp>


namespace medication::exporter::exceptions
{
ServerErrorException::ServerErrorException(const std::string& message, HttpStatus statusCode)
    : ClientException(message)
    , mStatusCode(statusCode)
{
}

HttpStatus ServerErrorException::statusCode() const
{
    return mStatusCode;
}
}


namespace
{
const rapidjson::Pointer accessTokenPtr{"/access_token"};
const rapidjson::Pointer expiresInPtr{"/expires_in"};
const rapidjson::Pointer tokenTypePtr{"/token_type"};


std::tuple<std::string, int> validateAccessTokenResponse(const model::NumberAsStringParserDocument& tokenDoc)
{
    const auto accessToken = tokenDoc.getOptionalStringValue(accessTokenPtr);
    const auto expiresIn = tokenDoc.getOptionalIntValue(expiresInPtr);
    const auto tokenType = tokenDoc.getOptionalStringValue(tokenTypePtr);

    ModelExpect(accessToken && not accessToken->empty(), "No access token set in response.");
    ModelExpect(expiresIn && *expiresIn > 0, "No expiration time set in response.");
    ModelExpect(tokenType && not tokenType->empty(), "No token type set in response.");

    return {std::string{*accessToken}, *expiresIn};
}
}


using namespace medication::exporter::exceptions;


OAuthClientBase::OAuthClientBase(std::string clientId, std::string host, std::string port,
                                 std::shared_ptr<CrlProvider> crlProvider)
    : mClientId(std::move(clientId))
    , mHost(std::move(host))
    , mPort(std::move(port))
    , mAccessTokenCache(std::make_unique<TokenCache>(TokenCache::TokenType::AccessToken))
    , mCrlProvider(std::move(crlProvider))
{
}

bool OAuthClientBase::testConnection() const
{
    try
    {
        [[maybe_unused]] const auto _ = getOrRefreshAccessToken();
        return true;
    }
    catch (const std::exception& e)
    {
        LOG(ERROR) << "Error testing connection to " << mHost << ":" << mPort << ": " << e.what();
    }
    return false;
}

std::string OAuthClientBase::getOrRefreshAccessToken() const
{
    A_27820.start("Check token expiration and request new when expired");
    {
        const std::lock_guard<std::mutex> lock(mTokenCacheMutex);
        if (mAccessTokenCache->isValid())
        {
            return mAccessTokenCache->getToken();
        }
    }
    A_27821.start("Request token");
    return fetchToken(mHost, mPort, accessTokenRequest(mHost, mPort), *mAccessTokenCache);
    A_27821.finish();
    A_27820.finish();
}

std::string OAuthClientBase::fetchToken(const std::string& host, const std::string& port, ClientRequest&& request,
                                        TokenCache& tokenCache) const
{
    auto client = createClient(host, port);
    BDEMessage message;
    try
    {
        message.update(
            BDEMessage::Data{.innerOperation = String::split(request.getHeader().target(), "?")[0],
                             .processor = "trezept",
                             .requestId = request.getHeader().header(Header::XRequestId).value_or("NOT SET")});
        auto response = client->send(request);
        message.update(BDEMessage::Data{
            .endTime = model::Timestamp::now(),
            .responseCode = static_cast<unsigned int>(toNumericalValue(response.getHeader().status()))});
        if (getTRezeptEvent())
        {
            message.update(BDEMessage::Data{.prescriptionId = getTRezeptEvent()->getPrescriptionId().toString()});
        }

        switch (response.getHeader().status())
        {
            case HttpStatus::OK: {
                auto doc = model::NumberAsStringParserDocument::fromJson(response.getBody());
                auto [token, expiresIn] = validateAccessTokenResponse(doc);
                {
                    const std::lock_guard<std::mutex> lock(mTokenCacheMutex);
                    if (! tokenCache.isValid())
                    {
                        TLOG(INFO) << "Update " << std::string{magic_enum::enum_name(tokenCache.getType())}
                                   << " for client " << mClientId << ", expiresIn=" << expiresIn;
                        tokenCache.setToken(token, std::chrono::seconds(expiresIn));
                    }
                    return tokenCache.getToken();
                }
            }
            case HttpStatus::Unauthorized:
                invalidateTokenCache();
                Fail2("Client with id " + mClientId + " not authorized for " +
                          std::string{magic_enum::enum_name(tokenCache.getType())} + " request.",
                      AuthenticationException);
            default:
                FailFhirVzd("Unexpected http status " + std::string{toString(response.getHeader().status())} +
                                " while fetching " + std::string{magic_enum::enum_name(tokenCache.getType())} +
                                " with client id " + mClientId,
                            response.getHeader().status());
        }
    }
    catch (const boost::beast::system_error& exc)
    {
        const auto msg = "Network error while fetching " + std::string{magic_enum::enum_name(tokenCache.getType())} +
                         " and client id " + mClientId + ", " + exc.code().message();
        message.update(BDEMessage::Data{.endTime = model::Timestamp::now(), .error = msg});
        Fail2(msg, NetworkException);
    }
    // Never reached
    return {};
}

ClientResponse OAuthClientBase::handleCommonHttpErrors(std::function<ClientResponse(BDEMessage& message)>&& action) const
{
    BDEMessage message;
    try
    {
        message.update(BDEMessage::Data{.processor = "trezept"});
        auto response = action(message);
        message.update(BDEMessage::Data{
                .endTime = model::Timestamp::now(),
                .responseCode = static_cast<unsigned int>(toNumericalValue(response.getHeader().status()))});
        if (getTRezeptEvent())
        {
            message.update(BDEMessage::Data{
                .lastModified = getTRezeptEvent()->getLastModified(),
                .prescriptionId = getTRezeptEvent()->getPrescriptionId().toString()});
        }

        switch (response.getHeader().status())
        {
            case HttpStatus::OK:
                return response;

            case HttpStatus::Unauthorized:
                invalidateTokenCache();
                Fail2("Client with id " + mClientId + " not authorized.", AuthenticationException);

            default:
                invalidateTokenCache();
                FailFhirVzd("Unexpected http status " + std::string{toString(response.getHeader().status())} +
                                " with client id " + mClientId,
                            response.getHeader().status());
        }
    }
    catch (const boost::beast::system_error& exc)
    {
        const auto msg = "Network error while searching with client with id " + mClientId + ", " + exc.code().message();
        message.update(BDEMessage::Data{.endTime = model::Timestamp::now(), .error = msg});
        Fail2(msg, NetworkException);
    }
}

void OAuthClientBase::invalidateTokenCache() const
{
    const std::lock_guard<std::mutex> lock(mTokenCacheMutex);
    mAccessTokenCache->invalidate();
}

std::unique_ptr<ClientInterface> OAuthClientBase::createClient(const std::string& hostname,
                                                               const std::string& port) const
{
    // GEMREQ-start A_27855
    return std::make_unique<HttpsClient>(HttpsClient{
        {.hostname = hostname,
         .port = port,
         .connectionTimeout = std::chrono::seconds(60),
         .resolveTimeout = std::chrono::seconds(5),
         .tlsParameters = TlsConnectionParameters{
             .certificateVerifier =
                 TlsCertificateVerifier::withCustomRootCertificates(
                     Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_TRUSTED_CAS))
                     .withCrl(*mCrlProvider)}}});
    // GEMREQ-end A_27855
}

std::string OAuthClientBase::getClientId() const
{
    return mClientId;
}

std::string OAuthClientBase::getHost() const
{
    return mHost;
}

std::string OAuthClientBase::getPort() const
{
    return mPort;
}

TokenCache& OAuthClientBase::getAccessTokenCache() const
{
    return *mAccessTokenCache;
}

std::mutex& OAuthClientBase::getTokenCacheMutex() const
{
    return mTokenCacheMutex;
}

const model::TRezeptEvent* OAuthClientBase::getTRezeptEvent() const
{
    return mEvent;
}

void OAuthClientBase::setEvent(const model::TRezeptEvent* event)
{
    mEvent = event;
}
