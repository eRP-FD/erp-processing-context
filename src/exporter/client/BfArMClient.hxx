/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "exporter/client/OAuthClientBase.hxx"

#include <memory>


class ClientInterface;
class TokenCache;

enum class HttpStatus;

namespace model
{
class ErpTPrescriptionCarbonCopy;
}


/**
 * @brief Entry point for BfArM backend services
 *
 * This class acts as a client for BfArM service calls. The auth token
 * is managed internally for receiving, storing and updating it when expired.
 *
 * Failure is indicated by throwing exceptions which must be handled properly.
 */
class BfArMClient : public OAuthClientBase
{
public:
    /**
     * @brief Create an HTTPS client for working with the BfArM backend.
     *
     * @param clientId The client identifier.
     */
    explicit BfArMClient(std::string clientId, std::shared_ptr<CrlProvider> crlProvider);

    ~BfArMClient() override;

    /**
     * @brief Transfer a carbon copy doc to BfArM.
     *
     * Automatically handles token refresh if needed.
     *
     * @param Bundle TPrescription document, as returned from the transformer.
     */
    void sendCarbonCopy(const model::ErpTPrescriptionCarbonCopy& doc) const;

protected:
    ClientRequest accessTokenRequest(const std::string& host, const std::string& port) const override;

private:
    const std::string mClientSecret;//!< Client secret
};
