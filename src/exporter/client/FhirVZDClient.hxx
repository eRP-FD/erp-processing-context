/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "exporter/client/OAuthClientBase.hxx"

#include <memory>

enum class HttpStatus;

namespace model
{
class Bundle;
}


/**
 * @brief Entry point for FHIR VZD backend services
 *
 * Ths class takes care of receiving access and authorization tokens. It manages the
 * refresh of the tokens in order to reduce service calls.
 * Failure is indicated by throwing exceptions which must be handled properly.
 */
class FhirVzdClient : public OAuthClientBase
{
public:
    /**
     * @brief Create an HTTPS client for working with the FHIR VZD backend.
     *
     * @param clientId The client identifier for push notifications.
     */
    explicit FhirVzdClient(std::string clientId, std::shared_ptr<CrlProvider> crlProvider, const std::string& xContextId);

    ~FhirVzdClient() override;

    /**
     * @brief Perform a FHIR search on the VZD backend.
     *
     * Automatically handles token refresh if needed.
     *
     * @returns Bundle with search results (resources, not validated)
     */
    model::Bundle performSearch(const std::string& orgId) const;

    bool testConnection() const override;

protected:
    ClientRequest accessTokenRequest(const std::string& host, const std::string& port) const override;

    void invalidateTokenCache() const override;

private:
    /**
     * @brief Provides validated access to the auth token.
     * If the token is expired, it requests a new token,
     * updates the internal token cache and returns it
     * when finished.
     *
     * @returns Auth token as string.
     * @throws When fetching the token fails.
     */
    std::string getOrRefreshAuthToken() const;

    /**
     * @brief Create the body content for the accessToken request.
     *
     * @returns Body content as string.
     * @throws When fetching the token fails.
     */
    std::string buildAccessTokenRequestBody() const;

    /**
     * @brief Perform a HTTP request to retrieve an authentication token.
     * The token is cached in the token cache of the instance.
     * Whenever the token is accessed, its expiration is considered.
     * If it is expired, a new HTTP request is performed, first.
     *
     * The access token is not validated within this method.
     *
     * @param accessToken A valid access token.
     *
     * @returns Auth token as string.
     */
    std::string fetchAuthToken(const std::string& accessToken) const;

    const std::string mClientSecret;                    //!< Client secret
    const std::string mApiUrl;                          //!< FHIR VZD base url
    const std::string mApiPort;                         //!< FHIR VZD base port
    mutable std::unique_ptr<TokenCache> mAuthTokenCache;//!< Token cache for the auth token
};
