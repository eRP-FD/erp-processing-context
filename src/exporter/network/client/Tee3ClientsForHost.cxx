/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "Tee3ClientsForHost.hxx"
#include "Tee3Client.hxx"
#include "Tee3ClientPool.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/Random.hxx"
#include "shared/util/TLog.hxx"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_future.hpp>
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
    auto executor = co_await boost::asio::this_coro::executor;
    auto resolver = Resolver{pool->mIoContext};
    auto [ec, endpointResult] = co_await resolver.async_resolve(
        mHostname, std::to_string(mPort), bind_executor(pool->mStrand, boost::asio::as_tuple(boost::asio::deferred)));
    std::unordered_set<boost::asio::ip::address> addresses;
    std::ranges::transform(endpointResult, std::inserter(addresses, addresses.begin()),
                           [](const Resolver::results_type::endpoint_type& endpoint) -> boost::asio::ip::address {
                               return endpoint.address();
                           });
    Expect3(pool->mStrand.running_in_this_thread(), "not running on strand.", std::logic_error);
    // if the ip address is not in our resolver list, we disconnect
    for (auto& tee3client : mAvailableClients)
    {
        if (! addresses.contains(tee3client->currentEndpoint().address()))
        {
            co_spawn(pool->mStrand, tee3client->close(), consign(boost::asio::detached, pool));
        }
    }
    auto now = std::chrono::steady_clock::now();
    std::ranges::transform(endpointResult, std::back_inserter(mEndpoints),
                           [&now](const Resolver::results_type::endpoint_type& endpoint) {
                               return Tee3Client::EndpointData{.endpoint = endpoint, .retryCount = 0, .nextRetry = now};
                           });

    co_await boost::asio::dispatch(boost::asio::bind_executor(executor, boost::asio::deferred));
}


boost::asio::awaitable<Tee3ClientsForHost::Tee3ClientPtr> Tee3ClientsForHost::acquire()
{
    auto pool = mOwningPool.lock();
    Expect3(pool, "called acquire during shutdown.", std::logic_error);
    // Take a token out of the client list, if it is empty, wait until one as been
    // freed again
    if (! mChannel->try_receive([](auto...) {
        }))
    {
        TVLOG(2) << "waiting for tee client on " << mHostname;
        co_await mChannel->async_receive(bind_executor(pool->mStrand, boost::asio::as_tuple(boost::asio::deferred)));
    }
    Expect3(pool->mStrand.running_in_this_thread(), "not running on strand.", std::logic_error);
    Expect(! mAvailableClients.empty(), "No tee3 client available");

    auto clientIt = Random::selectRandomElement(mAvailableClients.begin(), mAvailableClients.end());
    auto tee3Client = Tee3ClientPtr{clientIt->release(), Tee3ClientDeleter{pool->weak_from_this()}};
    mAvailableClients.erase(clientIt);
    co_await tee3Client->ensureConnected();
    co_return tee3Client;
}

boost::asio::awaitable<void> Tee3ClientsForHost::release(std::shared_ptr<Tee3ClientPool> pool,
                                                         std::unique_ptr<Tee3Client> instance)
{
    if (std::ranges::find(mEndpoints, instance->currentEndpoint(), &Tee3Client::EndpointData::endpoint) ==
        mEndpoints.end())
    {
        // refreshEndpoints has removed the instances endpoint
        co_await instance->close();
    }
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

boost::asio::awaitable<std::vector<Tee3Client::EndpointData>> Tee3ClientsForHost::async_endpoints() const
{
    co_return mEndpoints;
}

std::vector<Tee3Client::EndpointData> Tee3ClientsForHost::endpoints() const
{
    if (auto pool = mOwningPool.lock())
    {
        if (pool->mStrand.running_in_this_thread())
        {
            return mEndpoints;
        }
        return co_spawn(pool->mStrand, async_endpoints(), boost::asio::use_future).get();
    }
    LOG(WARNING) << "endpoint called during shutdown.";
    return {};
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
