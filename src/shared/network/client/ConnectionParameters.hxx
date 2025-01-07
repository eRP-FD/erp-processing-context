/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_SHARED_NETWORK_CLIENT_CONNECTIONPARAMETERS_HXX
#define ERP_SHARED_NETWORK_CLIENT_CONNECTIONPARAMETERS_HXX

#include "shared/network/client/TlsCertificateVerifier.hxx"
#include "shared/crypto/CertificateChainAndKey.hxx"

#include <chrono>
#include <optional>
#include <string>


/**
 * Structure for the TLS specific parameters needed to connect to a server.
 */
struct TlsConnectionParameters
{
    /**
     * TLS server certificate verifier.
     */
    TlsCertificateVerifier certificateVerifier;

    /**
     * Client certificate to use for TLS connections. Defaults to NONE.
     */
    std::optional<CertificateChainAndKey> clientCertificate = std::nullopt;

    /**
     * Optional ciphers to be used instead of the default ciphers.
     * The ciphers should be provided as one string with `:` separator.
     */
    std::optional<std::string> forcedCiphers = std::nullopt;

    /**
     * Whether certificate subject can be trusted or the subject alternate names (SANs) should be checked.
     * One related discussion: https://bugs.chromium.org/p/chromium/issues/detail?id=308330
     */
    bool trustCertificateCn = false;

    /**
     * Whether to enable TCP keep-alive. The parameters are designed to prevent recv() from blocking
     * indefinitely when the network connection is lost.
     *
     * If we want to support the same functionality for insecure HTTP requests, we should move this
     * into the ConnectionParameters parent struct.
     */
    bool tcpKeepAlive = false;
};


/**
 * Structure for the parameters needed to connect to a server.
 */
struct ConnectionParameters
{
    /**
     * Host name of server to be connected.
     */
    std::string hostname;

    /**
     * TCP port number of server to be connected.
     */
    std::string port;

    std::chrono::milliseconds connectionTimeout;

    /**
     * Timeout for resolving the endpoint via DNS
     */
    std::chrono::milliseconds resolveTimeout;

    /**
     * The TLS specific parameters. std::nullopt for plain TCP connections.
     */
    std::optional<TlsConnectionParameters> tlsParameters;
};


#endif
