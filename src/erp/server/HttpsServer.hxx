/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_HTTPSSERVER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_HTTPSSERVER_HXX

// "boost/asio.hpp" includes "winnt.h". "winnt.h" #defines the macro "DEFINE".
// "DEFINE" again as used as a symbol in the HttpHeader. To avoid a compilation error on Windows
// "asio.hpp" must be included before "HttpHeader" (which is included by "RequestHanderManager.hxx")
#include <boost/asio/ssl/context.hpp>

#include "erp/server/ThreadPool.hxx"
#include "erp/server/handler/RequestHandlerManager.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/SafeString.hxx"

#include <memory>
#include <optional>
#include <string>

class PcServiceContext;
class HttpsServer
{
public:
    // 0.0.0.0 means "all interfaces on the local machine", including local host and all ethernet drivers.
    // If a host has two IP addresses, and a server running on the host is configured to listen
    // on 0.0.0.0, it will be reachable at both of those IP addresses.
    static constexpr std::string_view defaultHost = "0.0.0.0";

    HttpsServer (
        const std::string_view address,
        uint16_t port,
        RequestHandlerManager&& requestHandlers,
        PcServiceContext& serviceContext,
        bool enforceClientAuthentication = false,
        const SafeString& caCertificates = SafeString());

    /**
     * Start to serve requests in a thread pool of the given size.
     * The current thread is *not* used to serve any requests.
     */
    void serve (size_t threadCount, std::string_view threadBaseName);
    void waitForShutdown (void);
    void shutDown (void);
    bool isStopped() const;
    ThreadPool& getThreadPool();
    PcServiceContext& serviceContext();

private:
    ThreadPool mThreadPool;
    boost::asio::ssl::context mSslContext;
    RequestHandlerManager mRequestHandlers;
    PcServiceContext& mServiceContext;
};

#endif
