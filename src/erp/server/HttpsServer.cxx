/*
* (C) Copyright IBM Deutschland GmbH 2021, 2024
* (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
*/

#include "erp/server/HttpsServer.hxx"
#include "erp/server/ServerSocketHandler.hxx"


HttpsServer::HttpsServer(const std::string_view address, const uint16_t port, RequestHandlerManager&& requestHandlers,
                         PcServiceContext& serviceContext, bool enforceClientAuthentication,
                         const SafeString& caCertificates)
    : BaseHttpsServer(address, port, enforceClientAuthentication, caCertificates)
    , mServiceContext(serviceContext)
{
    // Create and launch a listening port
    std::make_shared<ServerSocketHandler>(
        mThreadPool.ioContext(), mSslContext,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(address), port), std::move(requestHandlers),
        mServiceContext)
        ->run();
    TVLOG(0) << "listening on port " << port;
}

PcServiceContext& HttpsServer::serviceContext()
{
    return mServiceContext;
}
