/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_IMPLEMENTATION_TLSSESSION_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_IMPLEMENTATION_TLSSESSION_HXX

#include "erp/server/SslStream.hxx"
#include "erp/util/SafeString.hxx"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include <memory>
#include <optional>
#include <string>

class TlsSession
{
public:
    /**
     * Created TLS session with provided hostname and port. If the optional certificates are provided
     * they are loaded, otherwise the default root certificates are loded.
     * @param hostname
     * @param port
     * @param caCertificates
     */
    TlsSession (const std::string& hostname,
                const std::string& port,
                const uint16_t connectionTimeoutSeconds,
                bool enforceServerAuthentication,
                const SafeString& caCertificates,
                const SafeString& clientCertificate,
                const SafeString& clientPrivateKey,
                const std::optional<std::string>& forcedCiphers);

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
    void establish (const bool trustCn);

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

    const uint16_t mConnectionTimeoutSeconds;

    /* Boost.ASIO I/O context. A reference to this is required by most Beast/ASIO classes. */
    boost::asio::io_context mIoContext;

    /* An SSL context used for general configuration on which any TLS session will be based. */
    boost::asio::ssl::context mSslContext;

    /* The stream used for reading or writing any data over this TLS session. */
    SslStream mSslStream;

    /* A TLS session ticket used for resuming a previously established TLS connection. */
    std::unique_ptr<TlsSessionTicketImpl> mTicket;

    void loadRootCertificates (const SafeString& caCertificates);

    void configureContext (
        bool enforceServerAuthentication,
        const SafeString& caCertificates,
        const SafeString& clientCertificate,
        const SafeString& clientPrivateKey,
        const std::optional<std::string>& forcedCiphers);

    void configureSession(const bool trustCn);
};

#endif
