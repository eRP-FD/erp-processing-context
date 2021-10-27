/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_HTTPSCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_HTTPSCLIENT_HXX

#include "erp/client/ClientBase.hxx"
#include "erp/util/SafeString.hxx"

#include <cstdint>
#include <string>


class SslStream;


class HttpsClient
    : public ClientBase<SslStream>
{
public:
    /// Use this constructor to create a Https client using the given
    /// host name and port number with (optional) the given certificates.
    explicit HttpsClient (
        const std::string& host,
        std::uint16_t port,
        const uint16_t connectionTimeoutSeconds,
        bool enforceServerAuthentication = true,
        const SafeString& caCertificates = SafeString(),
        const SafeString& clientCertificate = SafeString(),
        const SafeString& clientPrivateKey = SafeString(),
        const std::optional<std::string>& forcedCiphers = std::nullopt);

    /// Use this constructor to convert from e.g. a TeeClient.
    explicit HttpsClient(ClientBase<SslStream>&& other);
};


#endif
