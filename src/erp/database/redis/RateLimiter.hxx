/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_RATELIMITER_HXX
#define ERP_PROCESSING_CONTEXT_RATELIMITER_HXX

#include "erp/service/RedisInterface.hxx"

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string_view>

class RateLimiter
{
public:
    /**
     * Setup a rate limiter instance with a constraint of allowing calls / timespan.
     * @param iface Redis / MockRedis interface
     * @param redisKeyPrefix Identification key for redis db
     * @param calls Allowed calls within timespan
     * @param timepan Timespan (period must be at least milliseconds.)
     */
    RateLimiter(std::shared_ptr<RedisInterface> iface, std::string_view redisKeyPrefix, size_t calls,
                std::chrono::duration<float> timespan);

    virtual ~RateLimiter() = default;

    /**
     * Reconfigure instance with allowed number of calls.
     * @param calls  Allowed calls within configuerd timespan
     */
    void setUpperLimitCalls(size_t calls);


    /**
     * Reconfigure instance with a new timespan
     * @param timespan Timespan (period must be at least milliseconds.)
     */
    void setTimespan(std::chrono::duration<float> timespan);


    /**
         * Add item into the storage if it's not yet there, otherwise
         * update the item.
         *
         * Return true if token may be further processed and false when token is marked as blocked.
         */
    bool
    updateCallsCounter(const std::string& sub,
                       const std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>& exp) const;

    /**
     * Get access to the base name of the key.
     * @return The key base name
     */
    std::string_view redisKeyPrefix() const;

    /**
     * Get the current time since epoch. Allow subclass to override this method in order to simulate
     * a time in the future, for unit tests.
     * @return Current time since epoch
     */
    virtual int64_t nowValue() const;

protected:
    std::shared_ptr<RedisInterface> mInterface;
    std::string_view mRedisKeyPrefix{"ERP-PC-UNCONFIGURED:"};
    size_t mNumCalls{0};
    std::chrono::duration<float> mTimespan{0};
};

#endif//ERP_PROCESSING_CONTEXT_RATELIMITER_HXX
