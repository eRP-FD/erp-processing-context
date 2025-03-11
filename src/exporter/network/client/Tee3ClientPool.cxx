/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */


#include "exporter/network/client/Tee3ClientPool.hxx"
#include "exporter/VauAutTokenSigner.hxx"
#include "exporter/network/client/Tee3Client.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/Random.hxx"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/beast/http/error.hpp>
#include <rapidjson/document.h>
#include <ranges>

namespace {

// GEMREQ-start A_24773#restart-graceful
bool isGracefulError(boost::system::error_code ec)
{
    return ec == Tee3Client::error::restart || ec == boost::asio::ssl::error::stream_truncated ||
           ec == boost::beast::http::error::end_of_stream || ec == boost::asio::error::connection_reset ||
           ec == boost::asio::error::not_connected;
}
// GEMREQ-end A_24773#restart-graceful

} // namespace


Tee3ClientPool::Tee3ClientPool(boost::asio::io_context& ctx, HsmPool& hsmPool,
                               TslManager& tslManager)
    : mIoContext{ctx}
    , mStrand{make_strand(mIoContext)}
    , mHsmPool{hsmPool}
    , mTslManager{tslManager}
{
}


boost::asio::awaitable<Tee3ClientPool::Tee3ClientPtr> Tee3ClientPool::acquire(std::string hostname)
{
    auto executor = co_await boost::asio::this_coro::executor;
    co_await boost::asio::dispatch(boost::asio::bind_executor(mStrand, boost::asio::deferred));
    if (! mTee3Clients.contains(hostname))
    {
        Fail("Host has not been added to pool");
    }

    // select one random client from our list
    auto& tee3ClientPool = mTee3Clients.at(hostname);
    auto tee3Client = co_await tee3ClientPool->acquire();

    co_await boost::asio::dispatch(boost::asio::bind_executor(executor, boost::asio::deferred));
    co_return tee3Client;
}


boost::asio::awaitable<boost::system::result<Tee3Client::Response>>
Tee3ClientPool::sendTeeRequest(std::string hostname, Tee3Client::Request req,
                               std::unordered_map<std::string, std::any> logDataBag)
{
    auto teeClient = co_await acquire(hostname);
    boost::system::result<Tee3Client::Response> resp;

    // GEMREQ-start A_24773#reconnect
    while (isGracefulError((resp = co_await teeClient->sendInner(req, logDataBag)).error()))
    {
        teeClient.reset();
        teeClient = co_await acquire(hostname);
    }
    // GEMREQ-end A_24773#reconnect
    co_return resp;
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


boost::asio::awaitable<boost::system::result<std::unique_ptr<Tee3ClientsForHost>>>
Tee3ClientPool::setupPool(std::string hostname, std::uint16_t port, size_t connectionCount)
{
    auto clientPool = std::make_unique<Tee3ClientsForHost>(shared_from_this(), hostname, port, connectionCount);
    co_await clientPool->init();
    co_return clientPool;
}
