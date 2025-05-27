/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/network/client/HttpsClientPool.hxx"
#include "shared/util/PeriodicTimer.hxx"
#include "shared/util/Random.hxx"

#include <algorithm>

class HttpsClientPool::DnsRefresher : public FixedIntervalHandler
{
public:
    DnsRefresher(HttpsClientPool& parent, const std::chrono::steady_clock::duration& interval)
        : FixedIntervalHandler{interval}
        , mParent{parent.weak_from_this()}
        , mResolver{parent.mIoContext}
    {
    }
    void timerHandler() final
    {
        if (auto parent = mParent.lock())
        {
            TLOG(INFO) << "Refreshing endpoints for: " << parent->hostname();
            mResolver.async_resolve(parent->hostname(), parent->mConnectionParams.port,
                                    std::bind_front(&HttpsClientPool::handleEndpoints, parent));
        }
    };
    std::weak_ptr<HttpsClientPool> mParent;
    Resolver mResolver;
};

HttpsClientPool::HttpsClientDeleter::HttpsClientDeleter(std::weak_ptr<HttpsClientPool> pool)
    : mPool{std::move(pool)}
{
}

void HttpsClientPool::HttpsClientDeleter::operator()(HttpsClient* httpsClient)
{
    std::unique_ptr<HttpsClient> asUnique(httpsClient);
    // ensure the pool is not destroyed while the instance is returned:
    if (auto pool = mPool.lock())
    {
        pool->release(std::move(asUnique));
    }
}


class HttpsClientPool::Construct : public HttpsClientPool
{
public:
    template<typename... Ts>
    Construct(Ts&&... args)
        : HttpsClientPool{std::forward<Ts>(args)...}
    {
    }
};

std::shared_ptr<HttpsClientPool> HttpsClientPool::create(gsl::not_null<boost::asio::io_context*> ioContext,
                                                         ConnectionParameters params, std::size_t maxConnections,
                                                         const duration& dnsRefreshInterval)
{
    auto newInstance =
        std::make_shared<HttpsClientPool::Construct>(std::move(ioContext), std::move(params), maxConnections, dnsRefreshInterval);
    newInstance->init();
    return newInstance;
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
HttpsClientPool::HttpsClientPool(gsl::not_null<boost::asio::io_context*> ioContext, ConnectionParameters params,
                                 std::size_t maxConnections, const duration& dnsRefreshInterval)
    : mIoContext{*ioContext}
    , mConnectionParams{std::move(params)}
    , mUnusedConnections{maxConnections}
    , mDnsRefreshinterval{dnsRefreshInterval}
{
}

void HttpsClientPool::init()
{
    boost::asio::io_context context;
    Resolver resolver{context};
    resolver.async_resolve(mConnectionParams.hostname, mConnectionParams.port,
                           std::bind_front(&HttpsClientPool::handleEndpoints, this));
    context.run();
    mDnsRefreshTimer = std::make_unique<PeriodicTimer<DnsRefresher>>(*this, mDnsRefreshinterval);
    mDnsRefreshTimer->start(mIoContext, mDnsRefreshinterval);
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
void HttpsClientPool::handleEndpoints(const boost::system::error_code& ec, Resolver::results_type results)
{
    std::unique_lock lock(mMutex);
    if (ec)
    {
        TLOG(WARNING) << "DNS lookup failed for: " << hostname() << ": " << ec.message() << " - keeping last entries";
    }
    else
    {
        std::set<boost::asio::ip::tcp::endpoint> oldEndpoints;
        swap(mEndpoints, oldEndpoints);
        std::ranges::move(results, std::inserter(mEndpoints, mEndpoints.begin()));
        std::set<boost::asio::ip::tcp::endpoint> removed;
        std::ranges::set_difference(std::move(oldEndpoints), mEndpoints, std::inserter(removed, removed.begin()));
        if (! removed.empty())
        {
            TLOG(INFO) << "Removed endpoints from " << hostname() << ": " << String::join(removed);
        }
    }
    TLOG(INFO) << "current endpoints for " << hostname() << ": " << String::join(mEndpoints);
}


std::string HttpsClientPool::hostname() const
{
    return mConnectionParams.hostname;
}


std::uint16_t HttpsClientPool::port() const
{
    return gsl::narrow<std::uint16_t>(std::stoi(mConnectionParams.port));
}

HttpsClientPool::HttpsClientPtr HttpsClientPool::acquireInternal()
{
    std::unique_lock lock(mMutex);
    mClientAvailableCondition.wait(lock, [this] {
        return ! mAvailableClients.empty() || mEndpoints.empty() || mUnusedConnections > 0;
    });
    if (mEndpoints.empty())
    {
        return {nullptr, HttpsClientDeleter{weak_from_this()}};
    }
    if (mAvailableClients.empty() && mUnusedConnections > 0)
    {
        --mUnusedConnections;
        auto randomEndpoint = Random::selectRandomElement(mEndpoints.begin(), mEndpoints.end());
        return HttpsClientPtr{new HttpsClient{*randomEndpoint, mConnectionParams},
                              HttpsClientDeleter{weak_from_this()}};
    }
    auto clientIt = Random::selectRandomElement(mAvailableClients.begin(), mAvailableClients.end());
    auto httpsClient = HttpsClientPtr{clientIt->release(), HttpsClientDeleter{weak_from_this()}};
    mAvailableClients.erase(clientIt);
    return httpsClient;
}

HttpsClientPool::HttpsClientPtr HttpsClientPool::acquire()
{
    auto httpsClient = acquireInternal();
    while (httpsClient && ! validEndpoint(httpsClient->currentEndpoint()))
    {
        httpsClient = acquireInternal();
    }
    return httpsClient;
}

void HttpsClientPool::release(std::unique_ptr<HttpsClient> httpsClient)
{
    std::unique_lock lock(mMutex);
    if (validEndpoint(httpsClient->currentEndpoint()))
    {
        mAvailableClients.push_back(std::move(httpsClient));
    }
    else
    {
        ++mUnusedConnections;
    }
    lock.unlock();
    mClientAvailableCondition.notify_one();
}

bool HttpsClientPool::validEndpoint(const std::optional<boost::asio::ip::tcp::endpoint>& endpoint) const
{
    return endpoint && mEndpoints.contains(*endpoint);
}
