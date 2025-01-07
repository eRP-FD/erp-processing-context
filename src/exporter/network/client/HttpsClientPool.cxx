/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/network/client/HttpsClientPool.hxx"
#include "shared/util/Random.hxx"

#include <algorithm>

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


HttpsClientPool::HttpsClientPool(ConnectionParameters params, std::size_t maxConnections)
: mConnectionParams{std::move(params)}
{
    auto endpointResult = Resolver{mIoContext}.resolve(mConnectionParams.hostname, mConnectionParams.port);
    std::ranges::copy(endpointResult.begin(),  endpointResult.end(), std::back_inserter(mEndpoints));
    for (std::size_t i = 0; i < maxConnections; ++i)
    {
        auto randomEndpoint = Random::selectRandomElement(endpointResult.begin(), endpointResult.end());
        mAvailableClients.push_back(std::make_unique<HttpsClient>(*randomEndpoint, mConnectionParams));
    }
}

std::string HttpsClientPool::hostname() const
{
    return mConnectionParams.hostname;
}


std::uint16_t HttpsClientPool::port() const
{
    return gsl::narrow<std::uint16_t>(std::stoi(mConnectionParams.port));
}

HttpsClientPool::HttpsClientPtr HttpsClientPool::acquire()
{
    std::unique_lock lock(mMutex);
    if (mAvailableClients.empty())
    {
        mClientAvailableCondition.wait(lock, [this] {
            return ! mAvailableClients.empty();
        });
    }
    auto clientIt = Random::selectRandomElement(mAvailableClients.begin(), mAvailableClients.end());
    auto httpsClient = HttpsClientPtr{clientIt->release(), HttpsClientDeleter{weak_from_this()}};
    mAvailableClients.erase(clientIt);
    return httpsClient;
}

void HttpsClientPool::release(std::unique_ptr<HttpsClient> httpsClient)
{
    std::unique_lock lock(mMutex);
    mAvailableClients.push_back(std::move(httpsClient));
    mClientAvailableCondition.notify_one();
}
