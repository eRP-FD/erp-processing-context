/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_IMPLEMENTATION_TLSSESSION_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_IMPLEMENTATION_TLSSESSION_HXX

#include "shared/network/client/ConnectionParameters.hxx"
#include "shared/network/connection/SslStream.hxx"
#include "shared/util/SafeString.hxx"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>

class TlsSession
{
public:
    /**
     * Created TLS session with provided hostname and port. If the optional certificates are provided
     * they are loaded, otherwise the default root certificates are loaded.
     * @param params connection and validation parameters
     */
    TlsSession (const ConnectionParameters& params);

    /**
     * Created TLS session with provided endpoint. If the optional certificates are provided
     * they are loaded, otherwise the default root certificates are loaded.
     * @param ep
     * @param params connection and validation parameters
     */
    TlsSession(const boost::asio::ip::tcp::endpoint& ep, const ConnectionParameters& params);

    ~TlsSession();

    /**
     * Attempts to provide a secure communication channel
     * with the server identified by `mHostName` and `mPort`.
     *
     * A TLS connection can be established by means of resumption
     * (from a previously successfully established and later torn-down connection) or by a full
     * TLS session negotiation (also known as the TLS handshake).
     *
     * The session will be automatically torn-down upon the destruction of
     * the `ClientResponse` object returned by `HttpsClient`'s "send/makeRequest"-like APIs.
     *
     * Therefore this method must be called on every fresh request to the server.
     */
    void establish();

    /**
     * Shuts down the currently established TLS connection.
     *
     * Should only be called after a successful call to `establish()`.
     *
     * Automatically called when a `ClientResponse` returned by `HttpsClient` is destroyed.
     */
    void teardown();

    /**
     * Provides client code with the TlsSession's internal SSL stream,
     * which is required for doing any I/O over the secure channel provided by this TlsSession
     *
     * @return internal SslStream object
     */
    SslStream& getSslStream ();

    /**
     * Currently only used for testing purposes
     *
     * @return bool -- whether the last established session
     * was resumed or full handshake took place
     */
    bool hasBeenResumed () const;

    /**
     * Saves and uses `session`'s TLS ticket for future communication
     */
    void inheritTicketFrom (const TlsSession& session);

private:
    class TlsSessionTicketImpl;

    /* Identifiers of the server we wish to establish a TlsSession with. */
    std::string mHostName;
    std::string mPort;

    std::optional<boost::asio::ip::tcp::endpoint> mForcedEndpoint;

    std::chrono::milliseconds mConnectionTimeout;

    /* Boost.ASIO I/O context. A reference to this is required by most Beast/ASIO classes. */
    boost::asio::io_context mIoContext;

    /* An SSL context used for general configuration on which any TLS session will be based. */
    boost::asio::ssl::context mSslContext;

    /* The stream used for reading or writing any data over this TLS session. */
    SslStream mSslStream;

    /* A TLS session ticket used for resuming a previously established TLS connection. */
    std::unique_ptr<TlsSessionTicketImpl> mTicket;

    TlsCertificateVerifier mCertificateVerifier;

    /**
     * Whether certificate subject can be trusted or the subject alternate names (SANs) should be checked.
     * One related discussion: https://bugs.chromium.org/p/chromium/issues/detail?id=308330
     */
    bool mTrustCn;

    void loadRootCertificates (const SafeString& caCertificates);

    void configureContext(const ConnectionParameters& params);

    void configureSession(const boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp>& resolverResults);
};

#endif
