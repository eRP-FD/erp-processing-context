/*
* (C) Copyright IBM Deutschland GmbH 2021
* (C) Copyright IBM Corp. 2021
*/

#include "erp/server/HttpsServer.hxx"

#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/ServerSocketHandler.hxx"
#include "erp/server/TlsSettings.hxx"
#include "erp/client/RootCertificatesMgr.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/TerminationHandler.hxx"


namespace
{
void configureSslContext (
   boost::asio::ssl::context& sslContext,
   SafeString&& serverCertificate,
   SafeString&& serverPrivateKey,
   bool enforceClientAuthentication,
   const SafeString& caCertificates)
{
   TlsSettings::restrictVersions(sslContext);
   TlsSettings::setAllowedCiphersAndCurves(sslContext, std::nullopt);

   if(enforceClientAuthentication)
   {
       sslContext.set_verify_mode(boost::asio::ssl::context::verify_fail_if_no_peer_cert | boost::asio::ssl::context::verify_peer);
       RootCertificatesMgr::loadCaCertificates(sslContext, caCertificates);
   }
   else
   {
       sslContext.set_verify_mode(boost::asio::ssl::verify_none);
   }

   sslContext.use_certificate_chain(boost::asio::buffer(serverCertificate, serverCertificate.size()));
   serverCertificate.safeErase();

   sslContext.use_private_key(boost::asio::buffer(serverPrivateKey, serverPrivateKey.size()), boost::asio::ssl::context::file_format::pem);
   serverPrivateKey.safeErase();
}
}


HttpsServer::HttpsServer (
   const std::string_view address,
   const uint16_t port,
   RequestHandlerManager&& requestHandlers,
   PcServiceContext& serviceContext,
   bool enforceClientAuthentication,
   const SafeString& caCertificates)
   : mThreadPool(),
   mSslContext(boost::asio::ssl::context::tlsv12),
   mRequestHandlers(),
   mServiceContext(serviceContext)
{
   TVLOG(1) << "Creating HTTPS Server, host=" << address << ", port=" << port << ", enforceClientAuthentication=" << enforceClientAuthentication;

   SafeString serverCertificate{Configuration::instance().getStringValue(ConfigurationKey::SERVER_CERTIFICATE)};
   SafeString serverPrivateKey{Configuration::instance().getStringValue(ConfigurationKey::SERVER_PRIVATE_KEY)};
   Expect(serverCertificate.size() > 0, "server certificate is required");
   Expect(serverPrivateKey.size() > 0, "server key is required");

   configureSslContext(mSslContext, std::move(serverCertificate), std::move(serverPrivateKey),
                       enforceClientAuthentication, caCertificates);

   // Create and launch a listening port
   std::make_shared<ServerSocketHandler>(
       mThreadPool.ioContext(),
       mSslContext,
       boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(address), port),
       std::move(requestHandlers),
       mServiceContext
       )->run();
   LOG(INFO) << "listening on port " << port;
}


void HttpsServer::serve (const size_t threadCount)
{
   Expect(threadCount > 0, "need at least 1 thread to serve");

   TLOG(INFO) << "serving requests with " << threadCount << " threads";
   mThreadPool.setUp(threadCount);
}


void HttpsServer::waitForShutdown (void)
{
   mThreadPool.joinAllThreads();
}


void HttpsServer::shutDown (void)
{
   mThreadPool.shutDown();
}


bool HttpsServer::isStopped() const
{
   return mThreadPool.ioContext().stopped();
}


ThreadPool& HttpsServer::getThreadPool()
{
   return mThreadPool;
}

PcServiceContext& HttpsServer::serviceContext()
{
   return mServiceContext;
}
