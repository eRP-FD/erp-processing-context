/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */
#ifndef MEDICATION_EXPORTER_HTTPSCLIENTPOOL_HXX
#define MEDICATION_EXPORTER_HTTPSCLIENTPOOL_HXX

#include "shared/network/client/HttpsClient.hxx"

#include <boost/core/noncopyable.hpp>
#include <condition_variable>
#include <cstdint>
#include <gsl/gsl-lite.hpp>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <string>

class PeriodicTimerBase;

class HttpsClientPool : public std::enable_shared_from_this<HttpsClientPool>, public boost::noncopyable
{
public:
    using duration = std::chrono::steady_clock::duration;
    using Resolver = boost::asio::ip::tcp::resolver;
    struct HttpsClientDeleter;
    using HttpsClientPtr = std::unique_ptr<HttpsClient, HttpsClientDeleter>;

    static std::shared_ptr<HttpsClientPool> create(gsl::not_null<boost::asio::io_context*> ioContext,
                                                   ConnectionParameters params, std::size_t maxConnections,
                                                   const duration& dnsRefreshInterval);

    HttpsClientPtr acquire();

    std::string hostname() const;
    std::uint16_t port() const;

private:
    class Construct;
    HttpsClientPool(gsl::not_null<boost::asio::io_context*> ioContext, ConnectionParameters params,
                    std::size_t maxConnections, const duration& dnsRefreshInterval);
    void init();


    class DnsRefresher;
    void handleEndpoints(const boost::system::error_code& ec, Resolver::results_type results);

    void release(std::unique_ptr<HttpsClient> HttpsClient);
    HttpsClientPtr acquireInternal();
    bool validEndpoint(const std::optional<boost::asio::ip::tcp::endpoint>& endpoint) const;

    std::mutex mMutex;
    boost::asio::io_context& mIoContext;
    ConnectionParameters mConnectionParams;
    size_t mUnusedConnections;
    duration mDnsRefreshinterval;
    std::unique_ptr<PeriodicTimerBase> mDnsRefreshTimer;
    std::condition_variable mClientAvailableCondition;
    std::list<std::unique_ptr<HttpsClient>> mAvailableClients;
    std::set<Resolver::results_type::endpoint_type> mEndpoints;
};

struct HttpsClientPool::HttpsClientDeleter {
    explicit HttpsClientDeleter(std::weak_ptr<HttpsClientPool> pool);

    void operator()(HttpsClient* httpsClient);

private:
    std::weak_ptr<HttpsClientPool> mPool;
};


#endif // MEDICATION_EXPORTER_HTTPSCLIENTPOOL_HXX
