/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "exporter/client/FhirVZDClient.hxx"

#include <chrono>


enum class HttpStatus;


class FhirVzdClientMock : public FhirVzdClient
{
public:
    explicit FhirVzdClientMock(std::string clientId, const std::string& xContextId);
    ~FhirVzdClientMock() override = default;

    static void setAccessTokenExpiration(std::chrono::seconds v)
    {
        mAccessTokenExpiration = v;
    }
    static void setAuthTokenExpiration(std::chrono::seconds v)
    {
        mAuthTokenExpiration = v;
    }
    static void setHttpStatus(HttpStatus status)
    {
        mHttpStatus = status;
    }
    static size_t getCallCount()
    {
        return mCallCounter;
    }
    static void resetCallCount()
    {
        mCallCounter = 0;
    }
protected:
    std::unique_ptr<UrlRequestSender> createClient() const override;
    static std::chrono::seconds mAccessTokenExpiration;
    static std::chrono::seconds mAuthTokenExpiration;
    static HttpStatus mHttpStatus;
    static size_t mCallCounter;
};
