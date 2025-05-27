/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/implementation/TlsSession.hxx"

#include "shared/crypto/Certificate.hxx"
#include "shared/crypto/OpenSsl.hxx"
#include "shared/crypto/OpenSslHelper.hxx"
#include "shared/network/Resolver.hxx"
#include "shared/network/connection/AsyncStreamHelper.hxx"
#include "shared/network/connection/RootCertificatesMgr.hxx"
#include "shared/network/connection/TlsSettings.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/TLog.hxx"

#include <boost/asio/ssl/error.hpp>
#include <boost/asio/error.hpp>

#include <boost/beast/core/error.hpp>
#include <boost/beast/core/tcp_stream.hpp>

#include <functional>
#include <memory>
#include <utility>

#ifdef _WIN32
#pragma warning( disable : 4459 )
#endif

namespace
{
    void checkForUngracefulTlsServerBehavior (std::function<void()>&& tryAction)
    {
        try
        {
            tryAction();
        }
        catch (const boost::beast::system_error& ex)
        {
            if (boost::asio::ssl::error::stream_truncated != ex.code() &&
                boost::asio::error::connection_reset      != ex.code() &&
                boost::asio::error::not_connected         != ex.code())
            {
                TLOG(WARNING) << "Beast ex: " << ex.what();
            }
        }
        catch (const std::exception& ex)
        {
            TLOG(ERROR) << "Std ex: " << ex.what();
        }
    }
}


class TlsSession::TlsSessionTicketImpl
{
public:
    explicit TlsSessionTicketImpl (const SslStream& sslStream);

    /**
     * Attempts to use this ticket for resuming a previously
     * successfully established (and later successfully torn-down) TLS connection.
     */
    void use ();

    /**
     * Tells whether this ticket has been used for the establishment of the last TLS connection
     *
     * Currently used for testing purposes only.
     *
     * @return bool
     */
    bool hasBeenUsed() const;

    /**
     * Retrieves the ticket from the currently
     * established TLS connection and saves it for future uses.
     */
    void save ();

    /**
     * Saves `ticket` for future uses (instead of retrieving it from the current session)
     */
    void save (const TlsSessionTicketImpl& other);

private:
    // "Further requirements for TLS-Connection"
    // GEMREQ-start GS-A_5322
    using SslSessionHandlePtr = std::unique_ptr<SSL_SESSION, decltype(&SSL_SESSION_free)>;
    // GEMREQ-end GS-A_5322

    const SslStream& mSslStream;
    SslSessionHandlePtr mSslSessionHandle;
};


TlsSession::TlsSessionTicketImpl::TlsSessionTicketImpl (const SslStream& sslStream)
: mSslStream{sslStream},
  mSslSessionHandle{nullptr, SSL_SESSION_free}
{
    /* Do not use `mSslStream` in this constructor as the internal stream is not yet created. */
}

// GEMREQ-start A_21269#ticketUseSave
void TlsSession::TlsSessionTicketImpl::use ()
{
    if (mSslSessionHandle && SSL_SESSION_is_resumable(mSslSessionHandle.get()))
    {
        SSL_set_session(mSslStream.getNativeHandle(), mSslSessionHandle.get());
    }
}


bool TlsSession::TlsSessionTicketImpl::hasBeenUsed() const
{
    return static_cast<bool>(SSL_session_reused(mSslStream.getNativeHandle()));
}


void TlsSession::TlsSessionTicketImpl::save ()
{
    if (!hasBeenUsed() &&
        SSL_get_session(mSslStream.getNativeHandle()) &&
        SSL_SESSION_is_resumable(SSL_get_session(mSslStream.getNativeHandle())))
    {
        /* BEWARE: SSL_get_session peeks at the session, while SSL_get1_session increments
                   its reference count (therefore requiring a later call to SSL_SESSION_free) */
        mSslSessionHandle.reset(SSL_get1_session(mSslStream.getNativeHandle()));
    }
}
// GEMREQ-end A_21269#ticketUseSave


void TlsSession::TlsSessionTicketImpl::save (const TlsSessionTicketImpl& other)
{
    // if the `other` ticket (belonging to the session's we're trying to inherit from)
    // contains an actual SSL session, then duplicate that and keep the copy in our ticket
    //
    if (other.mSslSessionHandle && SSL_SESSION_is_resumable(other.mSslSessionHandle.get()))
    {
        mSslSessionHandle.reset(SSL_SESSION_dup(other.mSslSessionHandle.get()));
    }
}


TlsSession::TlsSession(const ConnectionParameters& params)
    : mHostName{params.hostname},
      mPort{params.port},
      mConnectionTimeout(params.connectionTimeout),
      mIoContext{},
      // GEMREQ-start GS-A_4385
      // GEMREQ-start GS-A_5542
      mSslContext{boost::asio::ssl::context::tlsv12_client},
      // GEMREQ-end GS-A_4385
      // GEMREQ-end GS-A_5542
      mSslStream{},
      mTicket{},
      mCertificateVerifier{params.tlsParameters.value().certificateVerifier},
      mTrustCn{params.tlsParameters.value().trustCertificateCn}
{
    TVLOG(1) << "Creating TLS Session, host=" << params.hostname << ", port=" << params.port << ", conn. timeout=" << params.connectionTimeout
             << ", withClientCertificate=" << params.tlsParameters->clientCertificate.has_value();

    Expect(mConnectionTimeout.count() > 0, "Connection timeout must be greater than 0.");

    configureContext(params);
}

TlsSession::TlsSession(const boost::asio::ip::tcp::endpoint& ep, const ConnectionParameters& params)
    : TlsSession(params)
{
    mForcedEndpoint.emplace(ep);
}


/* pImpl via std::unique_ptr forces us to move the destructor past Impl's full definition */
TlsSession::~TlsSession () = default;


void TlsSession::establish()
{
    boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp> resolverResults;
    /* Look up the domain name. */
    if (! mForcedEndpoint.has_value())
    {
        resolverResults = boost::asio::ip::tcp::resolver{mIoContext}.resolve(mHostName, mPort);
    }
    else
    {
        resolverResults = boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp>::create(
            *mForcedEndpoint, mHostName, std::to_string(mForcedEndpoint->port()));
    }
    // In some cases it seems to be necessary to retry a handshake.
    constexpr size_t tryCount = 2;
    for (size_t index=0; index<tryCount; ++index)
    {
        try
        {
            // GEMREQ-start A_21269#ticketUse
            mSslStream = SslStream::create(mIoContext, mSslContext);
            mTicket = std::make_unique<TlsSessionTicketImpl>(mSslStream);

            configureSession(resolverResults);

            mTicket->use();

            mSslStream.handshake(boost::asio::ssl::stream_base::client);

            mTicket->save();
            // GEMREQ-end A_21269#ticketUse

            break;
        }
        catch(const boost::system::system_error& e)
        {
            if (e.code() == boost::beast::error::timeout)
            {
                TLOG(ERROR) << "timeout in TLS handshake (" << mHostName << ":" << mPort << ")";
                throw; // No retry for timeouts.
            }
            else if (index+1 == tryCount)
            {
                TLOG(ERROR) << "caught exception in TLS handshake (" << mHostName << ":" << mPort << ")";
                throw;
            }
            else
            {
                TLOG(WARNING) << "caught exception " << e.what() << " in TLS handshake; ignoring it and trying again  ("
                              << mHostName << ":" << mPort << ")";
            }
        }
        catch(...)
        {
            if (index+1 == tryCount)
            {
                TLOG(ERROR) << "caught exception in TLS handshake (" << mHostName << ":" << mPort << ")";
                throw;
            }
            else
                TLOG(WARNING) << "caught exception in TLS handshake; ignoring it and trying again (" << mHostName << ":"
                              << mPort << ")";
        }
    }
}


void TlsSession::teardown ()
{
    checkForUngracefulTlsServerBehavior([&stream = this->mSslStream]
                                        {
                                            stream.shutdown();
                                        });

    checkForUngracefulTlsServerBehavior([&socket = mSslStream.getLowestLayer().socket()]
                                        {
                                            socket.shutdown(
                                                 boost::asio::ip::tcp::socket::shutdown_both);
                                            socket.close();
                                        });
}


SslStream& TlsSession::getSslStream ()
{
    return mSslStream;
}


bool TlsSession::hasBeenResumed () const
{
    return mTicket->hasBeenUsed();
}


void TlsSession::inheritTicketFrom (const TlsSession& session)
{
    mTicket->save(*session.mTicket);
}

std::optional<boost::asio::ip::tcp::endpoint> TlsSession::currentEndpoint() const
{
    return mForcedEndpoint ? mForcedEndpoint : mSslStream.currentEndpoint();
}

void TlsSession::configureContext(const ConnectionParameters& params)
{
    mCertificateVerifier.install(mSslContext);

    TlsSettings::restrictVersions(mSslContext);
    TlsSettings::setAllowedCiphersAndCurves(mSslContext, params.tlsParameters.value().forcedCiphers);

    // GEMREQ-start GS-A_5526
    SSL_CTX_clear_options(mSslContext.native_handle(),
                          SSL_OP_LEGACY_SERVER_CONNECT | SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION);
    // GEMREQ-end GS-A_5526

    /* Setup client session caching. */
    SSL_CTX_set_session_cache_mode(mSslContext.native_handle(), SSL_SESS_CACHE_CLIENT);

    // "Further requirements for TLS-Connection"
    // GEMREQ-start GS-A_5322
    /* Set a timeout for the tickets. */
    SSL_CTX_set_timeout(mSslContext.native_handle(), 86400);
    // GEMREQ-end GS-A_5322

    // GEMREQ-start GS-A_5542
    SSL_CTX_set_info_callback(mSslContext.native_handle(), [](const SSL*, int context, int value)
    {
        if (context & SSL_CB_ALERT)
        {
            TVLOG(3) << ((context & SSL_CB_WRITE) ? "wrote" : "read")
                    << " TLS alert = "
                    << SSL_alert_desc_string_long(value)
                    << ", of type = "
                    << SSL_alert_type_string_long(value);
        }
    });
    // GEMREQ-end GS-A_5542

    if (params.tlsParameters.has_value() && params.tlsParameters->clientCertificate.has_value())
    {
        auto clientCertificate =
            params.tlsParameters->clientCertificate->certificateKeyPair.getCertificate();
        std::string certificateChainPem = Certificate(clientCertificate).toPem();
        if (! params.tlsParameters->clientCertificate->caCertificates.empty())
        {
            certificateChainPem += "\n";
            certificateChainPem += params.tlsParameters->clientCertificate->caCertificates;
        }
        mSslContext.use_certificate_chain(boost::asio::buffer(certificateChainPem));

        auto clientPrivateKey = params.tlsParameters->clientCertificate->certificateKeyPair.getKey();
        Expect(SSL_CTX_use_PrivateKey(mSslContext.native_handle(), clientPrivateKey.get()) != 0,
               "Failed adding client private key");
    }
}


void TlsSession::configureSession(const boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp>& resolverResults)
{
    /* Clear session - this allows us to reuse the same `mSslStream` for the same peer. */
    if (!SSL_clear(mSslStream.getNativeHandle()))
    {
        throw ExceptionWrapper<boost::beast::system_error>::create(
            {__FILE__, __LINE__}, static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category());
    }

    /* Set SNI Hostname (many hosts need this to handshake successfully). */
    if (!SSL_set_tlsext_host_name(mSslStream.getNativeHandle(), mHostName.c_str()))
    {
        throw ExceptionWrapper<boost::beast::system_error>::create(
            {__FILE__, __LINE__}, static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category());
    }

    /* Verify the remote server's certificate's subject alias names. */
    if (!SSL_set1_host(mSslStream.getNativeHandle(), mHostName.c_str()))
    {
        throw ExceptionWrapper<boost::beast::system_error>::create(
            {__FILE__, __LINE__}, static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category());
    }

    if (! mTrustCn)
    {
        /* Do not trust the certificate's subject, only the subject alternate names (SANs). */
        /* One related discussion: https://bugs.chromium.org/p/chromium/issues/detail?id=308330 */
        SSL_set_hostflags(mSslStream.getNativeHandle(), X509_CHECK_FLAG_NEVER_CHECK_SUBJECT);
    }

    mSslStream.getLowestLayer().expires_after(mConnectionTimeout);

    /* Make the connection on the IP address we get from a lookup. */
    boost::system::error_code errorCode =
        AsyncStreamHelper::connect(mSslStream.getLowestLayer(), mIoContext, resolverResults);
    if (errorCode)
    {
        throw ExceptionWrapper<boost::beast::system_error>::create({__FILE__, __LINE__}, errorCode);
    }
}
