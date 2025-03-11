/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/redis/TaskRateLimiter.hxx"
#include "shared/util/Configuration.hxx"

namespace
{
const std::string requestsCount = ":requests";
}

bool TaskRateLimiter::isBlockedForToday(RedisInterface& redisIfc, const std::string& hashedTelematikId)
{
    const int upperLimit = Configuration::instance().getIntValue(ConfigurationKey::SERVICE_TASK_GET_RATE_LIMIT);
    const auto key = hashedTelematikId + requestsCount;
    return (redisIfc.getIntValue(key) >= upperLimit);
}

void TaskRateLimiter::updateStatistic(RedisInterface& redisIfc, const std::string& hashedTelematikId)
{
    const auto key = hashedTelematikId + requestsCount;
    const auto count = redisIfc.incr(key);
    if (count == 1)
    {
        const auto exp_ms = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now() + std::chrono::days(1));
        redisIfc.setKeyExpireAt(key, exp_ms);
    }
}
