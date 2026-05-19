/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/network/message/HttpStatus.hxx"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>

class BDEMessage;
class ClientInterface;
class ClientResponse;
class ClientRequest;
class CrlProvider;
class TokenCache;
class UrlRequestSender;

enum class HttpStatus;


namespace model
{
    class TRezeptEvent;
    class Timestamp;
}

namespace medication::exporter::exceptions
{
/// @brief Base class for the BfArMClient exceptions.
class ClientException : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/// @brief Exception for http status 401
class AuthenticationException : public ClientException
{
public:
    using ClientException::ClientException;
};

/// @brief Exception for low level network (socket, ssl) errors
class NetworkException : public ClientException
{
public:
    using ClientException::ClientException;
};

/// @brief Exception for an http status not 200 or 401.
class ServerErrorException : public ClientException
{
public:
    using ClientException::ClientException;

    ServerErrorException(const std::string& message, HttpStatus statusCode);

    HttpStatus statusCode() const;

private:
    HttpStatus mStatusCode;
};

class TokenAcquisitionException : public ServerErrorException
{
 public:
   TokenAcquisitionException(const std::string& message, HttpStatus statusCode);
};

class OperationException : public ServerErrorException
{
public:
    OperationException(const std::string& message, HttpStatus statusCode);
};

//NOLINTNEXTLINE [cppcoreguidelines-macro-usage]
#define FailFhirVzd(exc, message, httpStatus) local::logAndThrow<exc>(message, fileAndLine, httpStatus)
}


class OAuthClientBase
{
public:
    OAuthClientBase(std::string clientId, std::string host, std::string port, std::shared_ptr<CrlProvider> crlProvider,
                    std::string xContextId);
    virtual ~OAuthClientBase() = default;

    virtual bool testConnection() const;

    /// @brief Pass an event object for logging purposes.
    /// The class won't own or modify the passed object. It
    /// is used for bde logging purposes.
    ///
    /// @param event A valid instance.
    void setEvent(const model::TRezeptEvent* event);

protected:
    virtual ClientRequest accessTokenRequest(const std::string& host, const std::string& port) const = 0;

    /// @brief Provides validated access to the access token.
    /// If the token is expired, it requests a new token,
    /// updates the internal token cache and returns it
    /// when finished.
    ///
    /// @return Access token as string.
    std::string getOrRefreshAccessToken() const;

    std::string fetchToken(const std::string& host, const std::string& port, ClientRequest&& request,
                           TokenCache& tokenCache) const;

    virtual void invalidateTokenCache() const;

    static constexpr auto defaultSuccess = [](HttpStatus status) {
        return status == HttpStatus::OK;
    };
    ClientResponse handleCommonHttpErrors(std::function<ClientResponse(BDEMessage& message)>&& action,
                                          const std::function<bool(HttpStatus)>& isSuccess = defaultSuccess) const;

    /// @brief Helper method for creating https clients.
    ///
    /// @param hostname Target host name to connect to.
    /// @param port Target port to connect to.
    /// @returns HttpsClient instance.
    virtual std::unique_ptr<UrlRequestSender> createClient() const;

    std::string getClientId() const;
    std::string getHost() const;
    std::string getPort() const;
    TokenCache& getAccessTokenCache() const;
    std::mutex& getTokenCacheMutex() const;

    const model::TRezeptEvent* getTRezeptEvent() const;
private:
    const std::string mClientId;//!< Clients push notification id
    const std::string mHost;
    const std::string mPort;
    mutable std::unique_ptr<TokenCache> mAccessTokenCache;//!< Token cache for the access token
    mutable std::mutex mTokenCacheMutex;
    const model::TRezeptEvent* mEvent{nullptr};
    std::shared_ptr<CrlProvider> mCrlProvider;
    std::string mXContextId;
};
