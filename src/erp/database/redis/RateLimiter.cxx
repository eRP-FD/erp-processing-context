/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/redis/RateLimiter.hxx"
#include "erp/service/RedisInterface.hxx"
#include "erp/util/JsonLog.hxx"
#include "erp/util/TLog.hxx"

#include <chrono>
#include <string>

RateLimiter::RateLimiter(std::shared_ptr<RedisInterface> iface, std::string_view redisKeyPrefix, size_t calls,
                         std::chrono::duration<float> timespan)
    : mInterface(std::move(iface))
    , mRedisKeyPrefix(redisKeyPrefix)
    , mNumCalls(calls)
    , mTimespan(timespan)
{
}

void RateLimiter::setUpperLimitCalls(size_t calls)
{
    mNumCalls = calls;
}

void RateLimiter::setTimespan(std::chrono::duration<float> timespan)
{
    mTimespan = timespan;
}

bool RateLimiter::updateCallsCounter(
    const std::string& sub,
    const std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>& exp) const
{
    using namespace std::chrono;
    const auto nowVal = nowValue();
    const auto spanVal = std::chrono::duration_cast<std::chrono::milliseconds>(mTimespan).count();
    const auto tBucket = nowVal - (nowVal % spanVal);
    const auto tUpper = tBucket + spanVal;

    const std::string baseKey{std::string{mRedisKeyPrefix} + ":" + sub};
    const std::string key{baseKey + ":" + std::to_string(tBucket)};

    try
    {
        if (mInterface->exists(baseKey))
        {
            // Key is already blocked.
            return false;
        }

        auto calls = mInterface->incr(key);
        if (calls == 1)
        {
            // Initial access, always pass.
            mInterface->setKeyExpireAt(key, exp);
        }
        else if (calls > static_cast<int>(mNumCalls) && nowVal < tUpper)
        {
            // Key will be blocked from now on.
            TVLOG(1) << "Access with given key blocked and will expire in about " << (exp.time_since_epoch().count() - nowVal) << " ms.";
            mInterface->setKeyFieldValue(baseKey, "", "");
            mInterface->setKeyExpireAt(baseKey, exp);
            return false;
        }
        else
        {
            // Allow access with given key.
            TVLOG(1) << "request #" << calls << " within " << spanVal << " ms.";
        }
        return true;
    }
    catch (const std::exception&)
    {
        JsonLog(LogId::DOS_ERROR, JsonLog::makeErrorLogReceiver())
            .message("DosHandler check failed due to previous errors, check skipped.");
        return true;
    }
    return false;
}

std::string_view RateLimiter::redisKeyPrefix() const
{
    return mRedisKeyPrefix;
}

int64_t RateLimiter::nowValue() const {
    using namespace std::chrono;
    const auto tNow = time_point_cast<milliseconds>(system_clock::now());
    const auto nowVal = tNow.time_since_epoch().count();
    return nowVal;
}
