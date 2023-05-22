/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/RedisClient.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/DurationConsumer.hxx"
#include "erp/util/String.hxx"

#include <sw/redis++/redis++.h>

using namespace std::literals::string_literals;

namespace
{

std::vector<std::pair<std::string, int>> extractSentinalHostsAndPorts(const std::string& hostsAndPortsStr)
{
    std::vector<std::pair<std::string, int>> result;
    const auto hostsAndPorts = String::split(hostsAndPortsStr, ',');
    Expect(!hostsAndPorts.empty(), "Empty result should not be possible");
    for(const auto& hostAndPortString : hostsAndPorts)
    {
        const auto hostAndPort = String::split(hostAndPortString, ':');
        Expect(hostAndPort.size() == 2, "Invalid sentinal host/port combination '" + hostAndPortString + "'");
        int port = 0;
        size_t pos = 0;
        try
        {
            port = std::stoi(hostAndPort[1], &pos);
        }
        catch(const std::exception& exc)
        {
            Fail("Invalid sentinel port '" + hostAndPort[1] + "'");
        }
        Expect(pos == hostAndPort[1].size(), "Invalid sentinel port '" + hostAndPort[1] + "' (trailing non-int characters)");
        result.emplace_back(hostAndPort[0], port);
    }
    return result;
}

} // anonymous namespace


RedisClient::RedisClient()
{
    const auto& configuration = Configuration::instance();
    TLOG(INFO) << "Initializing Redis Client.";

    const auto sentinelHosts = configuration.getOptionalStringValue(ConfigurationKey::REDIS_SENTINEL_HOSTS);
    const auto caCert = configuration.getOptionalStringValue(ConfigurationKey::REDIS_CERTIFICATE_PATH, "/erp/redis/REDIS_CERT");

    TVLOG(2) << "redis config:" << std::endl
        << "database=" << configuration.getIntValue(ConfigurationKey::REDIS_DATABASE) << std::endl
        << "port=" << configuration.getIntValue(ConfigurationKey::REDIS_PORT) << std::endl
        << "host=" << configuration.getStringValue(ConfigurationKey::REDIS_HOST) << std::endl
        << "sentinelHosts=" << (sentinelHosts ? *sentinelHosts : "<not set>") << std::endl
        << "sentinelMasterName="
        << configuration.getOptionalStringValue(ConfigurationKey::REDIS_SENTINEL_MASTER_NAME, "<not set>") << std::endl
        << "user=" << configuration.getStringValue(ConfigurationKey::REDIS_USER) << std::endl
        << "password=<password>" << std::endl
        << "connectionPoolSize=" << configuration.getIntValue(ConfigurationKey::REDIS_CONNECTIONPOOL_SIZE) << std::endl
        << "cert_path=" << caCert;

    using namespace sw::redis;
    sw::redis::tls::disable_auto_init();

    const auto connectTimeout =
        std::chrono::milliseconds(configuration.getOptionalIntValue(ConfigurationKey::REDIS_CONNECTION_TIMEOUT, 200));
    mOptions.db = configuration.getIntValue(ConfigurationKey::REDIS_DATABASE);
    mOptions.tls.enabled = true;
    mOptions.connect_timeout = connectTimeout;
    mOptions.user = configuration.getStringValue(ConfigurationKey::REDIS_USER);
    mOptions.password = configuration.getStringValue(ConfigurationKey::REDIS_PASSWORD);
    mOptions.tls.cacert = caCert;
    mPoolOptions.size = gsl::narrow<size_t>(configuration.getIntValue(ConfigurationKey::REDIS_CONNECTIONPOOL_SIZE));
    mPoolOptions.wait_timeout = std::chrono::milliseconds(0); // Block requesting thread on wait.
    mPoolOptions.connection_lifetime = std::chrono::milliseconds(0); // Never expire connections.

    if(sentinelHosts)
    {
        // "sentinel" mode
        TLOG(INFO) << "Connecting in sentinel mode.";
        mOptions.socket_timeout = std::chrono::milliseconds(
            configuration.getOptionalIntValue(ConfigurationKey::REDIS_SENTINEL_SOCKET_TIMEOUT, 200));
        sw::redis::SentinelOptions mSentinelOptions;
        mSentinelOptions.nodes = extractSentinalHostsAndPorts(*sentinelHosts);
        mSentinelOptions.connect_timeout = connectTimeout;
        mSentinelOptions.tls.enabled = true;
        mSentinelOptions.tls.cacert = caCert;
        mConnection = std::make_unique<sw::redis::Redis>(Redis(
            std::make_shared<sw::redis::Sentinel>(mSentinelOptions),
            configuration.getStringValue(ConfigurationKey::REDIS_SENTINEL_MASTER_NAME),
            Role::MASTER, mOptions, mPoolOptions));
    }
    else
    {
         // "normal" mode
        TLOG(INFO) << "Connecting in standard mode.";
        mOptions.port = configuration.getIntValue(ConfigurationKey::REDIS_PORT);
        mOptions.host = configuration.getStringValue(ConfigurationKey::REDIS_HOST);
        mConnection = std::make_unique<sw::redis::Redis>(Redis(mOptions, mPoolOptions));
    }
}

bool RedisClient::exists(const std::string_view& key)
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(
        DurationConsumer::categoryRedis, String::concatenateItems("Redis:exists(", key, ")"));
    return mConnection->exists(key);
}

std::optional<std::string> RedisClient::fieldValueForKey(const std::string_view& key,
                                                         const std::string_view& field)
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(
        DurationConsumer::categoryRedis, String::concatenateItems("Redis:fieldValueForKey(", key, ",", field, ")"));
    return mConnection->hget(key, field);
}

bool RedisClient::hasKeyWithField(const std::string_view& key, const std::string_view& field)
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(
        DurationConsumer::categoryRedis, String::concatenateItems("Redis:hasKeyWithField(", key, ",", field, ")"));
    return mConnection->hexists(key, field);
}

void RedisClient::setKeyFieldValue(const std::string_view& key, const std::string_view& field,
                                   const std::string_view& value)
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(
        DurationConsumer::categoryRedis, String::concatenateItems("Redis:setKeyFieldValue(", key, ",", field, ")"));
    using namespace sw::redis;
    mConnection->hset(key, field, value);
}

void RedisClient::setKeyExpireAt(
    const std::string_view& key,
    const std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>& timestamp)
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(
        DurationConsumer::categoryRedis, String::concatenateItems("Redis:setKeyExpireAt(", key, ")"));
    mConnection->pexpireat(key, timestamp);
}

int64_t RedisClient::incr(const std::string_view& key)
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(
        DurationConsumer::categoryRedis, String::concatenateItems("Redis:incr(", key, ")"));
    return mConnection->incr(key);
}

void RedisClient::publish(const std::string_view& channel, const std::string_view& message)
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(
        DurationConsumer::categoryRedis, String::concatenateItems("Redis:publish(", channel, ")"));
    mConnection->publish(channel, message);
}
