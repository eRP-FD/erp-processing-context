/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/client/implementation/TlsSession.hxx"

#include "erp/client/RootCertificatesMgr.hxx"
#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/server/TlsSettings.hxx"
#include "erp/util/AsyncStreamHelper.hxx"
#include "erp/util/OpenSsl.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/Expect.hxx"

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
                TLOG(ERROR) << "Beast ex: " << ex.what();
            }
        }
        catch (const std::exception& ex)
        {
            TLOG(ERROR) << "Std ex: " << ex.what();
        }
    }

    void logSslVerifyError(boost::asio::ssl::verify_context& context)
    {
        const int error = X509_STORE_CTX_get_error(context.native_handle());
        const int errorDepth = X509_STORE_CTX_get_error_depth(context.native_handle());
        X509* errCert = X509_STORE_CTX_get_current_cert(context.native_handle());
        char buffer[256];
        X509_NAME_oneline(X509_get_subject_name(errCert), buffer, sizeof buffer);
        TVLOG(1) << "error " << error << " (" <<  X509_verify_cert_error_string(error)
                << " at depth " << errorDepth << " in " << buffer;

        auto certificateMemory = shared_BIO::make();
        if (!PEM_write_bio_X509(certificateMemory, errCert))
        {
            TVLOG(1) << "can not convert certificate to PEM string";
        }
        else
        {
            char* data = nullptr;
            const size_t length = BIO_get_mem_data(certificateMemory, &data);
            std::string errCertPem(data, length);
            TVLOG(1) << "problem certificate:\n" << errCertPem << "\n\n";
        }
    }
    /**
     * This is a callback registered by openSsl for debugging to get the detailed information regarding
     * certificate verification problems. The callback does not affect the verification,
     * it only logs the data related to the problematic certificate.
     */
    [[maybe_unused]] bool verifyCertificateCallback (const bool succeded, boost::asio::ssl::verify_context& context)
    {
        char subjectName[256];

        X509* cert = X509_STORE_CTX_get_current_cert(context.native_handle());
        X509_NAME_oneline(X509_get_subject_name(cert), subjectName, sizeof subjectName);
        TVLOG(1) << "verifying certificate " << subjectName << ", " << (succeded ? " has succeeded" : " has failed");

        if ( ! succeded)
        {
           logSslVerifyError(context);
        }

        return succeded;
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


TlsSession::TlsSession(
    const std::string& hostname,
    const std::string& port,
    const uint16_t connectionTimeoutSeconds,
    bool enforceServerAuthentication,
    const SafeString& caCertificates,
    const SafeString& clientCertificate,
    const SafeString& clientPrivateKey,
    const std::optional<std::string>& forcedCiphers)
    : mHostName{hostname},
      mPort{port},
      mConnectionTimeoutSeconds(connectionTimeoutSeconds),
      mIoContext{},
      // GEMREQ-start GS-A_4385
      // GEMREQ-start GS-A_5542
      mSslContext{boost::asio::ssl::context::tlsv12_client},
      // GEMREQ-end GS-A_4385
      // GEMREQ-end GS-A_5542
      mSslStream{},
      mTicket{}
{
    TVLOG(1) << "Creating TLS Session, host=" << hostname << ", port=" << port << ", conn. timeout=" << connectionTimeoutSeconds
             << ", enforceServerAuthentication=" << enforceServerAuthentication;

    Expect(mConnectionTimeoutSeconds > 0, "Connection timeout must be greater than 0.");

    configureContext(enforceServerAuthentication, caCertificates, clientCertificate, clientPrivateKey, forcedCiphers);
}


/* pImpl via std::unique_ptr forces us to move the destructor past Impl's full definition */
TlsSession::~TlsSession () = default;


void TlsSession::establish (const bool trustCn)
{
    // In some cases it seems to be necessary to retry a handshake.
    constexpr size_t tryCount = 2;
    for (size_t index=0; index<tryCount; ++index)
    {
        try
        {
            mSslStream = SslStream::create(mIoContext, mSslContext);
            mTicket = std::make_unique<TlsSessionTicketImpl>(mSslStream);

            configureSession(trustCn);

            mTicket->use();

            mSslStream.handshake(boost::asio::ssl::stream_base::client);

            mTicket->save();

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


void TlsSession::loadRootCertificates (const SafeString& caCertificates)
{
    RootCertificatesMgr::loadCaCertificates(mSslContext, caCertificates);

#if 0
    // this callback is registered to better debug certificate verification problems,
    // it is not necessary for production and should be disabled
    boost::beast::error_code ec{};
    mSslContext.set_verify_callback(verifyCertificateCallback, ec);
#endif
}


void TlsSession::configureContext(
    bool enforceServerAuthentication,
    const SafeString& caCertificates,
    const SafeString& clientCertificate,
    const SafeString& clientPrivateKey,
    const std::optional<std::string>& forcedCiphers)
{
    if(!enforceServerAuthentication)
    {
        mSslContext.set_verify_mode(boost::asio::ssl::verify_none);
    }
    else
    {
        // "erp-Frontend des Versicherten: verpflichtende Zertifikatsprüfung"
        // GEMREQ-start A_15872
        // GEMREQ-start A_17124
        mSslContext.set_verify_mode(boost::asio::ssl::verify_peer);
        // GEMREQ-end A_17124
        // GEMREQ-end A_15872
    }

    if(clientCertificate.size())
    {
        Expect(clientPrivateKey.size() > 0, "Missing private key");
        mSslContext.use_certificate_chain(boost::asio::buffer((const char*)clientCertificate, clientCertificate.size()));
        mSslContext.use_private_key(boost::asio::buffer((const char*)clientPrivateKey, clientPrivateKey.size()),
                                    boost::asio::ssl::context::file_format::pem);
    }

    TlsSettings::restrictVersions(mSslContext);
    TlsSettings::setAllowedCiphersAndCurves(mSslContext, forcedCiphers);

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

    /* When verifying peer's certificate, use the bundled root certificates. */
    if(enforceServerAuthentication)
        loadRootCertificates(caCertificates);
}


void TlsSession::configureSession (const bool trustCn)
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

    if (!trustCn)
    {
        /* Do not trust the certificate's subject, only the subject alternate names (SANs). */
        /* One related discussion: https://bugs.chromium.org/p/chromium/issues/detail?id=308330 */
        SSL_set_hostflags(mSslStream.getNativeHandle(), X509_CHECK_FLAG_NEVER_CHECK_SUBJECT);
    }

    /* Look up the domain name. */
    auto const dnsLookupResults = boost::asio::ip::tcp::resolver{mIoContext}.
                                            resolve(mHostName.c_str(), mPort.c_str());

    mSslStream.getLowestLayer().expires_after(std::chrono::seconds(mConnectionTimeoutSeconds));

    /* Make the connection on the IP address we get from a lookup. */
    boost::system::error_code errorCode =
        AsyncStreamHelper::connect(mSslStream.getLowestLayer(), mIoContext, dnsLookupResults);
    if (errorCode)
    {
        throw ExceptionWrapper<boost::beast::system_error>::create({__FILE__, __LINE__}, errorCode);
    }
}
