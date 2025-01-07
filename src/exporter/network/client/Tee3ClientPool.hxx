/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */
#ifndef MEDICATION_EXPORTER_TEE3CLIENTPOOL_HXX
#define MEDICATION_EXPORTER_TEE3CLIENTPOOL_HXX

#include "Tee3ClientsForHost.hxx"
#include "exporter/network/client/Tee3Client.hxx"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/core/noncopyable.hpp>
#include <boost/system/result.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class HsmPool;
class Tee3Client;
class TslManager;


class Tee3ClientPool : public std::enable_shared_from_this<Tee3ClientPool>, public boost::noncopyable
{
public:
    using Tee3ClientPtr = Tee3ClientsForHost::Tee3ClientPtr;
    Tee3ClientPool(boost::asio::io_context& ctx, HsmPool& hsmPool, TslManager& tslManager);

    /**
     * Acquire a tee client from the pool. To return it to the pool,
     * simply destroy it or call reset()
     * The returned TEE client will already be connected and has performed the
     * TEE handshake and the authorization request.
     * If there are more clients acquired than available, the function will
     * wait until a TEE client is released.
     */
    boost::asio::awaitable<Tee3ClientPtr> acquire(std::string hostname);

    /**
     * wait for a free tee3 client and use that to send a TEE request
     * to the given endpoint.
     * In case the server mandates a TEE reconnect, it is executed automatically.
     */
    boost::asio::awaitable<boost::system::result<Tee3Client::Response>> sendTeeRequest(std::string hostname, Tee3Client::Request req, std::unordered_map<std::string, std::any>& logDataBag);

    /**
     * Add a connection pool for the given hostname, it must be called before trying
     * to acquire any TEE clients.
     *
     * This function is not thread-safe.
     */
    boost::asio::awaitable<void> addEpaHost(std::string hostname, std::uint16_t port, size_t connectionCount);

private:
    void release(std::unique_ptr<Tee3Client> tee3Client);
    static boost::asio::awaitable<void> internalRelease(std::weak_ptr<Tee3ClientPool> self, std::unique_ptr<Tee3Client> teeClient);
    boost::asio::awaitable<void> refreshEndpoints(std::string hostname, std::uint16_t port);
    boost::asio::awaitable<boost::system::result<std::unique_ptr<Tee3ClientsForHost>>>
    setupPool(std::string hostname, std::uint16_t port, size_t connectionCount);

    friend struct Tee3ClientDeleter;

    boost::asio::io_context& mIoContext;
    boost::asio::strand<boost::asio::any_io_executor> mStrand;
    HsmPool& mHsmPool;
    TslManager& mTslManager;
    // map of hostname -> tee clients, note that it does not make sense
    // to distinguish by port, as they would end up at the same ePA gateway
    std::unordered_map<std::string, std::unique_ptr<Tee3ClientsForHost>> mTee3Clients;

    friend class Tee3ClientsForHost;
};

#endif// MEDICATION_EXPORTER_TEE3CLIENTPOOL_HXX
