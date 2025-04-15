/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_BASEHTTPSSERVER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_BASEHTTPSSERVER_HXX

// "boost/asio.hpp" includes "winnt.h". "winnt.h" #defines the macro "DEFINE".
// "DEFINE" again as used as a symbol in the HttpHeader. To avoid a compilation error on Windows
// "asio.hpp" must be included before "HttpHeader" (which is included by "RequestHanderManager.hxx")
#include "shared/server/ThreadPool.hxx"
#include "shared/util/SafeString.hxx"

#include <boost/asio/ssl/context.hpp>

class BaseHttpsServer
{
public:
    // 0.0.0.0 means "all interfaces on the local machine", including local host and all ethernet drivers.
    // If a host has two IP addresses, and a server running on the host is configured to listen
    // on 0.0.0.0, it will be reachable at both of those IP addresses.
    static constexpr std::string_view defaultHost = "0.0.0.0";

    BaseHttpsServer(const std::string_view address, uint16_t port, bool enforceClientAuthentication = false,
                    const SafeString& caCertificates = SafeString());
    virtual ~BaseHttpsServer() = default;
    /**
     * Start to serve requests in a thread pool of the given size.
     * The current thread is *not* used to serve any requests.
     */
    void serve(size_t threadCount, std::string_view threadBaseName);
    void waitForShutdown(void);
    void shutDown(void);
    bool isStopped() const;
    ThreadPool& getThreadPool();

protected:
    ThreadPool mThreadPool;
    boost::asio::ssl::context mSslContext;
};

#endif
