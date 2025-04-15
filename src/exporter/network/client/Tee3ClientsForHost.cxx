/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/ExporterRequirements.hxx"
#include "Tee3ClientsForHost.hxx"
#include "Tee3Client.hxx"
#include "Tee3ClientPool.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/Random.hxx"
#include "shared/util/String.hxx"
#include "shared/util/TLog.hxx"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_future.hpp>
#include <ranges>
#include <unordered_set>


void Tee3ClientsForHost::Tee3ClientDeleter::operator()(Tee3Client* tee3Client)
{
    std::unique_ptr<Tee3Client> asUnique(tee3Client);
    // ensure the pool is not destroyed while the instance is returned:
    if (auto pool = mPool.lock())
    {
        auto* owningClientsForHost = asUnique->owningClientsForHost();
        VLOG(3) << "returned tee client on " << owningClientsForHost->hostname() << ", "
                << tee3Client->currentEndpoint();
        co_spawn(pool->mStrand, owningClientsForHost->release(pool, std::move(asUnique)), boost::asio::detached);
    }
}

Tee3ClientsForHost::Tee3ClientsForHost(const gsl::not_null<std::shared_ptr<Tee3ClientPool>>& owningPool,
                                       std::string initHostname, uint16_t initPort, size_t connectionCount)
    : mOwningPool{owningPool}
    , mHostname{std::move(initHostname)}
    , mPort{initPort}
    , mChannel{std::make_unique<Tee3ClientsForHost::Channel>(owningPool->mIoContext)}
    , mConnectionCount{connectionCount}
{
}

boost::asio::awaitable<boost::system::error_code> Tee3ClientsForHost::init()
{
    auto pool = mOwningPool.lock();
    Expect3(pool, "called init during shutdown.", std::logic_error);
    auto resolver = Resolver{pool->mIoContext};

    auto [ec, endpointResult] = co_await resolver.async_resolve(
        mHostname, std::to_string(mPort), boost::asio::as_tuple(bind_executor(pool->mStrand, boost::asio::deferred)));
    if (ec)
    {
        co_return ec;
    }
    Expect3(pool->mStrand.running_in_this_thread(), "not running on strand.", std::logic_error);
    for (std::size_t i = 0; i < mConnectionCount; ++i)
    {
        mAvailableClients.emplace_back(std::make_unique<Tee3Client>(pool->mIoContext, this));
        mChannel->async_send(boost::system::error_code{}, consign(boost::asio::detached, pool));
    }
    auto now = std::chrono::steady_clock::now();
    std::ranges::transform(endpointResult, std::back_inserter(mEndpoints),
                           [&now](const Resolver::results_type::endpoint_type& endpoint) {
                               return Tee3Client::EndpointData{.endpoint = endpoint, .retryCount = 0, .nextRetry = now};
                           });
    TLOG(INFO) << "resolved " << mEndpoints.size() << " endpoints for: " << mHostname << ":" << mPort;
    co_return boost::system::error_code{};
}


boost::asio::awaitable<void> Tee3ClientsForHost::refreshEndpoints()
{
    auto pool = mOwningPool.lock();
    Expect3(pool, "called refreshEndpoints during shutdown.", std::logic_error);
    Expect3(pool->mStrand.running_in_this_thread(), "not running on strand", std::logic_error);
    TLOG(INFO) << "Refreshing endpoints for: " << mHostname;
    A_25938.start("fetch new DNS entries");
    auto resolver = Resolver{pool->mIoContext};
    auto [ec, endpointResult] = co_await resolver.async_resolve(
        mHostname, std::to_string(mPort), bind_executor(pool->mStrand, boost::asio::as_tuple(boost::asio::deferred)));
    auto now = std::chrono::steady_clock::now();
    std::vector<Tee3Client::EndpointData> oldEndpoints;
    swap(mEndpoints, oldEndpoints);
    std::ranges::transform(endpointResult, std::back_inserter(mEndpoints),
                           [&now](const Resolver::results_type::endpoint_type& endpoint) {
                               return Tee3Client::EndpointData{.endpoint = endpoint, .retryCount = 0, .nextRetry = now};
                           });
    A_25938.finish();
    const auto isStillActive = [this](const Tee3Client::EndpointData& old) {
        auto equalsOld = std::bind_front(std::ranges::equal_to{},std::cref(old.endpoint));
        return std::ranges::any_of(mEndpoints, equalsOld, &Tee3Client::EndpointData::endpoint);
    };
    const auto asEndpoint = std::views::transform(&Tee3Client::EndpointData::endpoint);
    if (auto stillActive = std::ranges::remove_if(oldEndpoints, isStillActive);
        stillActive.begin() != oldEndpoints.begin())
    {
        std::ranges::subrange removed{oldEndpoints.begin(), stillActive.begin()};
        TLOG(INFO) << "removed endpoints from " << mHostname << ": " << String::join(removed | asEndpoint);
    }
    TLOG(INFO) << "current endpoints for " << mHostname << ": " << String::join(mEndpoints | asEndpoint);
}

boost::asio::awaitable<Tee3ClientsForHost::Tee3ClientPtr> Tee3ClientsForHost::acquire()
{
    auto pool = mOwningPool.lock();
    Expect3(pool, "called acquire during shutdown.", std::logic_error);
    Expect3(pool->mStrand.running_in_this_thread(), "not running on strand.", std::logic_error);
    for(;;) {
        Tee3ClientsForHost::Tee3ClientPtr client = co_await acquireInternal(pool);
        A_25938.start("ensure connections to outdated endpoints are not reused");
        if (! client->isTlsConnected() ||
            std::ranges::any_of(mEndpoints, [&client](const Tee3Client::EndpointData& activeEndpoint) -> bool {
                return client->currentEndpoint() == activeEndpoint.endpoint;
            }))
        {
            co_await client->ensureConnected();
            co_return client;
        }
        // `client` is connected to an endpoint that has been removed during refreshEndpoints.
        // It will be closed by Tee3ClientsForHost::release before returning it to pool
        A_25938.finish();
    }
}


boost::asio::awaitable<Tee3ClientsForHost::Tee3ClientPtr>
Tee3ClientsForHost::acquireInternal(std::shared_ptr<Tee3ClientPool> pool)
{
    Expect3(pool, "called acquire during shutdown.", std::logic_error);
    // Take a token out of the client list, if it is empty, wait until one as been freed again
    if (! mChannel->try_receive([](auto...) {
        }))
    {
        TVLOG(2) << "waiting for tee client on " << mHostname;
        co_await mChannel->async_receive(bind_executor(pool->mStrand, boost::asio::as_tuple(boost::asio::deferred)));
    }
    Expect(! mAvailableClients.empty(), "No tee3 client available");

    auto clientIt = Random::selectRandomElement(mAvailableClients.begin(), mAvailableClients.end());
    auto tee3Client = Tee3ClientPtr{clientIt->release(), Tee3ClientDeleter{pool->weak_from_this()}};
    mAvailableClients.erase(clientIt);
    co_return tee3Client;
}

boost::asio::awaitable<void> Tee3ClientsForHost::release(std::shared_ptr<Tee3ClientPool> pool,
                                                         std::unique_ptr<Tee3Client> instance)
{
    A_25938.start("close connections to outdated endpoints");
    if (std::ranges::find(mEndpoints, instance->currentEndpoint(), &Tee3Client::EndpointData::endpoint) ==
        mEndpoints.end())
    {
        TLOG(INFO) << "DNS entry for TEE3 connection to " << mHostname
                   << " was removed; closing: " << instance->currentEndpoint();
        co_await instance->closeTlsSession();
    }
    A_25938.finish();
    Expect3(pool->mStrand.running_in_this_thread(), "not running on strand.", std::logic_error);
    mAvailableClients.push_back(std::move(instance));
    mChannel->async_send(boost::system::error_code{}, consign(boost::asio::detached, pool));
}

std::string Tee3ClientsForHost::hostname() const
{
    return mHostname;
}

uint16_t Tee3ClientsForHost::port() const
{
    return mPort;
}

boost::asio::awaitable<std::vector<Tee3Client::EndpointData>> Tee3ClientsForHost::endpoints() const
{
    if (auto pool = mOwningPool.lock())
    {
        if (pool->mStrand.running_in_this_thread())
        {
            co_return mEndpoints;
        }
        co_return co_await co_spawn(
            pool->mStrand,
            [this]() -> boost::asio::awaitable<std::vector<Tee3Client::EndpointData>> {
                co_return mEndpoints;
            },
            bind_executor(co_await boost::asio::this_coro::executor));
    }
    LOG(WARNING) << "endpoint called during shutdown.";
    co_return std::vector<Tee3Client::EndpointData>{};
}


std::optional<std::string> Tee3ClientsForHost::vauNP() const
{
    return mVauNP;
}

void Tee3ClientsForHost::vauNP(std::optional<std::string> newVauNP)
{
    if (mVauNP != newVauNP)
    {
        if (newVauNP)
        {
            TLOG(INFO) << "Updated VAU-NP for " << mHostname << ":" << mPort << ": " << newVauNP.value();
        }
        else
        {
            TLOG(INFO) << "Removed VAU-NP for " << mHostname << ":" << mPort;
        }
    }
    mVauNP = std::move(newVauNP);
}

HsmPool& Tee3ClientsForHost::hsmPool()
{
    auto pool = mOwningPool.lock();
    Expect(pool, "Tee3ClientsForHost::hsmPool called on shutdown.");
    return pool->mHsmPool;
}


TslManager& Tee3ClientsForHost::tslManager()
{
    auto pool = mOwningPool.lock();
    Expect(pool, "Tee3ClientsForHost::tslManager called on shutdown.");
    return pool->mTslManager;
}
