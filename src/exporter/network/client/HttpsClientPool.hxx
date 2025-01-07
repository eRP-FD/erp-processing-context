/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */
#ifndef MEDICATION_EXPORTER_HTTPSCLIENTPOOL_HXX
#define MEDICATION_EXPORTER_HTTPSCLIENTPOOL_HXX

#include "shared/network/client/HttpsClient.hxx"

#include <boost/core/noncopyable.hpp>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <string>


class HttpsClientPool : public std::enable_shared_from_this<HttpsClientPool>, public boost::noncopyable
{
public:
    using Resolver = boost::asio::ip::tcp::resolver;
    struct HttpsClientDeleter;
    using HttpsClientPtr = std::unique_ptr<HttpsClient, HttpsClientDeleter>;

    HttpsClientPool(ConnectionParameters params, std::size_t maxConnections);

    HttpsClientPtr acquire();

    std::string hostname() const;
    std::uint16_t port() const;

private:
    void release(std::unique_ptr<HttpsClient> HttpsClient);

    std::mutex mMutex;
    std::condition_variable mClientAvailableCondition;
    std::list<std::unique_ptr<HttpsClient>> mAvailableClients;
    ConnectionParameters mConnectionParams;
    boost::asio::io_context mIoContext;
    std::vector<Resolver::results_type::endpoint_type> mEndpoints;
};

struct HttpsClientPool::HttpsClientDeleter {
    explicit HttpsClientDeleter(std::weak_ptr<HttpsClientPool> pool);

    void operator()(HttpsClient* httpsClient);

private:
    std::weak_ptr<HttpsClientPool> mPool;
};


#endif // MEDICATION_EXPORTER_HTTPSCLIENTPOOL_HXX
