/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "exporter/client/FhirVZDClient.hxx"
#include "shared/network/client/ClientInterface.hxx"

#include <chrono>


enum class HttpStatus;
struct ConnectionParameters;


class HttpsClientMock : public ClientInterface
{
public:
    explicit HttpsClientMock();

    ClientResponse send(const ClientRequest& clientRequest) override;

    static void setAccessTokenExpiration(std::chrono::seconds v)
    {
        mAccessTokenExpiration = v;
    }
    static void setAuthTokenExpiration(std::chrono::seconds v)
    {
        mAuthTokenExpiration = v;
    }

    static size_t getCallCount()
    {
        return mCallCounter;
    }

    static void resetCallCount()
    {
        mCallCounter = 0;
    }
    static void setHttpStatus(HttpStatus status)
    {
        mHttpStatus = status;
    }
    bool testConnection() override
    {
        return true;
    }

private:
    static std::chrono::seconds mAccessTokenExpiration;
    static std::chrono::seconds mAuthTokenExpiration;
    static size_t mCallCounter;
    static HttpStatus mHttpStatus;
};


class FhirVzdClientMock : public FhirVzdClient
{
public:
    explicit FhirVzdClientMock(std::string clientId);
    ~FhirVzdClientMock() override = default;

protected:
    std::unique_ptr<ClientInterface> createClient(const std::string& hostname, const std::string& port) const override;
};
