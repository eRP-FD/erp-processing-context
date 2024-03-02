/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_HTTPSCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_HTTPSCLIENT_HXX

#include "erp/client/ClientBase.hxx"
#include "erp/util/SafeString.hxx"

#include <boost/asio/ip/tcp.hpp>
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
        std::chrono::milliseconds resolveTimeout,
        bool enforceServerAuthentication = true,
        const SafeString& caCertificates = SafeString(),
        const SafeString& clientCertificate = SafeString(),
        const SafeString& clientPrivateKey = SafeString(),
        const std::optional<std::string>& forcedCiphers = std::nullopt);

    /// Use this constructor to create a Https client using a
    /// specific endpoint
    explicit HttpsClient (
        const boost::asio::ip::tcp::endpoint& ep,
        const std::string& host,
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
