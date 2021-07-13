#include "erp/client/HttpsClient.hxx"


HttpsClient::HttpsClient (
    const std::string& host,
    const std::uint16_t port,
    const uint16_t connectionTimeoutSeconds,
    bool enforceServerAuthentication,
    const SafeString& caCertificates,
    const SafeString& clientCertificate,
    const SafeString& clientPrivateKey,
    const std::optional<std::string>& forcedCiphers)
    : ClientBase<SslStream>(host, port, connectionTimeoutSeconds, enforceServerAuthentication,
                            caCertificates, clientCertificate, clientPrivateKey, forcedCiphers)
{
}


HttpsClient::HttpsClient (ClientBase<SslStream>&& other)
    : ClientBase<SslStream>(std::move(other))
{
}
