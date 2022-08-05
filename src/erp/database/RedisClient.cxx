/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/database/RedisClient.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/DurationConsumer.hxx"
#include "erp/util/String.hxx"


#include <sw/redis++/redis++.h>

using namespace std::literals::string_literals;


RedisClient::RedisClient()
{
    const auto& configuration = Configuration::instance();
    TLOG(WARNING) << "Initializing Redis Client.";
    VLOG(2) << "redis config:" << std::endl
        << "database=" << configuration.getIntValue(ConfigurationKey::REDIS_DATABASE) << std::endl
        << "port=" << configuration.getIntValue(ConfigurationKey::REDIS_PORT) << std::endl
        << "host=" << configuration.getStringValue(ConfigurationKey::REDIS_HOST) << std::endl
        << "user=" << configuration.getStringValue(ConfigurationKey::REDIS_USER) << std::endl
        << "password=<password>" << std::endl
        << "connectionPoolSize=" << configuration.getIntValue(ConfigurationKey::REDIS_CONNECTIONPOOL_SIZE) << std::endl
        << "cert_path=" << configuration.getOptionalStringValue(ConfigurationKey::REDIS_CERTIFICATE_PATH, "/erp/redis/CERT");

    using namespace sw::redis;
    sw::redis::tls::disable_auto_init();
    mOptions.db = configuration.getIntValue(ConfigurationKey::REDIS_DATABASE);
    mOptions.tls.enabled = true;
    mOptions.connect_timeout = std::chrono::milliseconds(configuration.getOptionalIntValue(ConfigurationKey::REDIS_CONNECTION_TIMEOUT, 200));
    mOptions.port = configuration.getIntValue(ConfigurationKey::REDIS_PORT);
    mOptions.host = configuration.getStringValue(ConfigurationKey::REDIS_HOST);
    mOptions.user = configuration.getStringValue(ConfigurationKey::REDIS_USER);
    mOptions.password = configuration.getStringValue(ConfigurationKey::REDIS_PASSWORD);
    mOptions.tls.cacert = configuration.getOptionalStringValue(ConfigurationKey::REDIS_CERTIFICATE_PATH, "/erp/redis/REDIS_CERT");
    mPoolOptions.size = configuration.getIntValue(ConfigurationKey::REDIS_CONNECTIONPOOL_SIZE);
    mPoolOptions.wait_timeout = std::chrono::milliseconds(0); // Block requesting thread on wait.
    mPoolOptions.connection_lifetime = std::chrono::milliseconds(0); // Never expire connections.
    mConnection = std::make_unique<sw::redis::Redis>(Redis(mOptions, mPoolOptions));
}

bool RedisClient::exists(const std::string_view& key)
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(String::concatenateItems("Redis:exists(", key, ")"));
    return mConnection->exists(key);
}

std::optional<std::string> RedisClient::fieldValueForKey(const std::string_view& key,
                                                         const std::string_view& field)
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(String::concatenateItems("Redis:fieldValueForKey(", key, ",", field, ")"));
    return mConnection->hget(key, field);
}

bool RedisClient::hasKeyWithField(const std::string_view& key, const std::string_view& field)
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(String::concatenateItems("Redis:hasKeyWithField(", key, ",", field, ")"));
    return mConnection->hexists(key, field);
}

void RedisClient::setKeyFieldValue(const std::string_view& key, const std::string_view& field,
                                   const std::string_view& value)
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(String::concatenateItems("Redis:setKeyFieldValue(", key, ",", field, ")"));
    using namespace sw::redis;
    mConnection->hset(key, field, value);
}

void RedisClient::setKeyExpireAt(
    const std::string_view& key,
    const std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>& timestamp)
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(String::concatenateItems("Redis:setKeyExpireAt(", key, ")"));
    mConnection->pexpireat(key, timestamp);
}

int64_t RedisClient::incr(const std::string_view& key)
{
    return mConnection->incr(key);
}

void RedisClient::publish(const std::string_view& channel, const std::string_view& message)
{
    mConnection->publish(channel, message);
}
