/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/server/BaseHttpsServer.hxx"
#include "shared/network/connection/RootCertificatesMgr.hxx"
#include "shared/network/connection/TlsSettings.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/TLog.hxx"


namespace
{
void configureSslContext(boost::asio::ssl::context& sslContext, SafeString&& serverCertificate,
                         SafeString&& serverPrivateKey, bool enforceClientAuthentication,
                         const SafeString& caCertificates)
{
    TlsSettings::restrictVersions(sslContext);
    TlsSettings::setAllowedCiphersAndCurves(sslContext, std::nullopt);

    if (enforceClientAuthentication)
    {
        sslContext.set_verify_mode(boost::asio::ssl::context::verify_fail_if_no_peer_cert |
                                   boost::asio::ssl::context::verify_peer);
        RootCertificatesMgr::loadCaCertificates(sslContext, caCertificates);
    }
    else
    {
        sslContext.set_verify_mode(boost::asio::ssl::verify_none);
    }

    sslContext.use_certificate_chain(boost::asio::buffer(serverCertificate, serverCertificate.size()));
    serverCertificate.safeErase();

    sslContext.use_private_key(boost::asio::buffer(serverPrivateKey, serverPrivateKey.size()),
                               boost::asio::ssl::context::file_format::pem);
    serverPrivateKey.safeErase();
}
};


BaseHttpsServer::BaseHttpsServer(const std::string_view address, uint16_t port,
                                 bool enforceClientAuthentication /* = false */,
                                 const SafeString& caCertificates /* = SafeString() */)
    : mThreadPool()
    , mSslContext(boost::asio::ssl::context::tlsv12)
{
    TVLOG(1) << "Creating HTTPS Server, host=" << address << ", port=" << port
             << ", enforceClientAuthentication=" << enforceClientAuthentication;

    SafeString serverCertificate{Configuration::instance().getStringValue(ConfigurationKey::SERVER_CERTIFICATE)};
    SafeString serverPrivateKey{Configuration::instance().getStringValue(ConfigurationKey::SERVER_PRIVATE_KEY)};
    Expect(serverCertificate.size() > 0, "server certificate is required");
    Expect(serverPrivateKey.size() > 0, "server key is required");

    configureSslContext(mSslContext, std::move(serverCertificate), std::move(serverPrivateKey),
                        enforceClientAuthentication, caCertificates);
}

void BaseHttpsServer::serve(const size_t threadCount, std::string_view threadBaseName)
{
    Expect(threadCount > 0, "need at least 1 thread to serve");

    TVLOG(0) << "serving requests with " << threadCount << " threads";
    mThreadPool.setUp(threadCount, threadBaseName);
}

void BaseHttpsServer::waitForShutdown(void)
{
    mThreadPool.joinAllThreads();
}

void BaseHttpsServer::shutDown(void)
{
    mThreadPool.shutDown();
}

bool BaseHttpsServer::isStopped() const
{
    return mThreadPool.ioContext().stopped();
}

ThreadPool& BaseHttpsServer::getThreadPool()
{
    return mThreadPool;
}
