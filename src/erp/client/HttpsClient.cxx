/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/client/HttpsClient.hxx"


HttpsClient::HttpsClient (
    const std::string& host,
    const std::uint16_t port,
    const uint16_t connectionTimeoutSeconds,
    std::chrono::milliseconds resolveTimeout,
    bool enforceServerAuthentication,
    const SafeString& caCertificates,
    const SafeString& clientCertificate,
    const SafeString& clientPrivateKey,
    const std::optional<std::string>& forcedCiphers)
    : ClientBase<SslStream>(host, port, connectionTimeoutSeconds, resolveTimeout, enforceServerAuthentication,
                            caCertificates, clientCertificate, clientPrivateKey, forcedCiphers)
{
}

HttpsClient::HttpsClient(const boost::asio::ip::tcp::endpoint& ep, const std::string& host,
                         const uint16_t connectionTimeoutSeconds, bool enforceServerAuthentication,
                         const SafeString& caCertificates, const SafeString& clientCertificate,
                         const SafeString& clientPrivateKey, const std::optional<std::string>& forcedCiphers)
    : ClientBase<SslStream>(ep, host, connectionTimeoutSeconds, enforceServerAuthentication, caCertificates,
                            clientCertificate, clientPrivateKey, forcedCiphers)
{
}


HttpsClient::HttpsClient (ClientBase<SslStream>&& other)
    : ClientBase<SslStream>(std::move(other))
{
}
