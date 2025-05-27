/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */


#include "exporter/ExporterRequirements.hxx"
#include "exporter/network/client/Tee3Client.hxx"
#include "exporter/network/client/Tee3ClientPool.hxx"
#include "exporter/VauAutTokenSigner.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/Random.hxx"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/beast/http/error.hpp>
#include <rapidjson/document.h>
#include <ranges>

std::shared_ptr<Tee3ClientPool> Tee3ClientPool::create(boost::asio::io_context& ctx, HsmPool& hsmPool,
                                                       TslManager& tslManager,
                                                       std::chrono::steady_clock::duration endpointRefreshInterval)
{
    std::shared_ptr<Tee3ClientPool> instance{new Tee3ClientPool{ctx, hsmPool, tslManager, endpointRefreshInterval}};
    if (endpointRefreshInterval > std::chrono::steady_clock::duration::zero())
    {
        instance->setupRefreshEndpointsTimer();
    }
    return instance;
}


Tee3ClientPool::Tee3ClientPool(boost::asio::io_context& ctx, HsmPool& hsmPool,
                               TslManager& tslManager, std::chrono::steady_clock::duration endpointRefreshInterval)
    : mIoContext{ctx}
    , mStrand{make_strand(mIoContext)}
    , mHsmPool{hsmPool}
    , mTslManager{tslManager}
    , mEndpointsRefreshInterval{endpointRefreshInterval}
    , mRefreshEndpointsTimer{mStrand}
{
}


boost::asio::awaitable<Tee3ClientPool::Tee3ClientPtr> Tee3ClientPool::acquire(std::string hostname)
{
    auto executor = co_await boost::asio::this_coro::executor;
    co_await boost::asio::dispatch(boost::asio::bind_executor(mStrand, boost::asio::deferred));
    auto tee3ClientPool = mTee3Clients.find(hostname);
    Expect(tee3ClientPool != mTee3Clients.end(), "Host has not been added to pool");
    auto tee3Client = co_await tee3ClientPool->second->acquire();
    co_await boost::asio::dispatch(boost::asio::bind_executor(executor, boost::asio::deferred));
    co_return tee3Client;
}


boost::asio::awaitable<void> Tee3ClientPool::addEpaHost(std::string hostname, std::uint16_t port, size_t connectionCount)
{
    LOG(INFO) << "setup pool for hostname " << hostname;
    auto pool = co_await setupPool(hostname, port, connectionCount);
    while (pool.has_error())
    {
        boost::asio::steady_timer timer{mIoContext, Tee3Client::retryTimeout};
        auto [ec] = co_await timer.async_wait(boost::asio::as_tuple(boost::asio::deferred));
        if (ec.failed())
        {
            TLOG(INFO) << "tee 3 client stopping connect: " << ec.message() << ": " << hostname << ":" << port;
        }
        pool = co_await setupPool(hostname, port, connectionCount);
    }
    mTee3Clients.emplace(hostname, std::move(pool.value()));
}

void Tee3ClientPool::setupRefreshEndpointsTimer()
{
    A_25938.start("setup timer for periodic refresh");
    mRefreshEndpointsTimer.expires_after(mEndpointsRefreshInterval);
    mRefreshEndpointsTimer.async_wait(std::bind_front(&Tee3ClientPool::refreshEndpoints, weak_from_this()));
    A_25938.finish();
}

void Tee3ClientPool::refreshEndpoints(const std::weak_ptr<Tee3ClientPool>& weakSelf,
                                      const boost::system::error_code& ec)
{
    if (ec.failed())
    {
        return;
    }
    if (auto self = weakSelf.lock())
    {
        Expect3(self->mStrand.running_in_this_thread(), "Tee3ClientPool::refreshEndpoints: not running on strand",
                std::logic_error);
        for (auto& epaHost: self->mTee3Clients | std::views::values)
        {
            co_spawn(self->mStrand, epaHost->refreshEndpoints(), consign(boost::asio::detached, self));
        }
        self->setupRefreshEndpointsTimer();
    }
}

boost::asio::awaitable<boost::system::result<std::unique_ptr<Tee3ClientsForHost>>>
Tee3ClientPool::setupPool(std::string hostname, std::uint16_t port, size_t connectionCount)
{
    auto clientPool = std::make_unique<Tee3ClientsForHost>(shared_from_this(), hostname, port, connectionCount);
    co_await clientPool->init();
    co_return clientPool;
}
