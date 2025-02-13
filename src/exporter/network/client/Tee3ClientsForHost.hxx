/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef MEDICATION_EXPORTER_TEE3_CLIENTS_FOR_HOST_HXX
#define MEDICATION_EXPORTER_TEE3_CLIENTS_FOR_HOST_HXX

#include "exporter/network/client/Tee3Client.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/core/noncopyable.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>
#include <list>
#include <memory>
#include <vector>

class HsmPool;
class Tee3Client;
class Tee3ClientPool;
class TslManager;

class Tee3ClientsForHost : public boost::noncopyable
{
public:
    using Resolver = boost::asio::ip::tcp::resolver;
    using Channel = boost::asio::experimental::concurrent_channel<void(boost::system::error_code)>;
    struct Tee3ClientDeleter;
    using Tee3ClientPtr = std::unique_ptr<Tee3Client, Tee3ClientDeleter>;

    Tee3ClientsForHost(const gsl::not_null<std::shared_ptr<Tee3ClientPool>>& owningPool, std::string initHostname,
                       uint16_t initPort, size_t connectionCount);

    boost::asio::awaitable<boost::system::error_code> init();

    boost::asio::awaitable<void> refreshEndpoints();

    boost::asio::awaitable<Tee3ClientsForHost::Tee3ClientPtr> acquire();

    std::string hostname() const;
    uint16_t port() const;

    boost::asio::awaitable<std::vector<Tee3Client::EndpointData>> endpoints() const;

    [[nodiscard]] std::optional<std::string> vauNP() const;

    void vauNP(std::optional<std::string> newVauNP);

    HsmPool& hsmPool();
    TslManager& tslManager();

private:
    boost::asio::awaitable<void> release(std::shared_ptr<Tee3ClientPool> pool, std::unique_ptr<Tee3Client> instance);

    std::weak_ptr<Tee3ClientPool> mOwningPool;
    std::string mHostname;
    uint16_t mPort;
    std::list<std::unique_ptr<Tee3Client>> mAvailableClients;
    std::vector<Tee3Client::EndpointData> mEndpoints;
    // for each message in the channel, there is one tee client
    // available. Use a pointer, Channel seems not to be movable
    std::unique_ptr<Channel> mChannel;
    std::optional<std::string> mVauNP{};
    size_t mConnectionCount;
};

struct Tee3ClientsForHost::Tee3ClientDeleter {
    explicit Tee3ClientDeleter(std::weak_ptr<Tee3ClientPool> pool)
        : mPool{std::move(pool)}
    {
    }

    void operator()(Tee3Client* tee3Client);

private:
    std::weak_ptr<Tee3ClientPool> mPool;
};


#endif// MEDICATION_EXPORTER_TEE3_CLIENTS_FOR_HOST_HXX
