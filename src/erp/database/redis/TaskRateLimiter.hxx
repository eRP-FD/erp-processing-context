/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TASKRATELIMITER_HXX
#define ERP_PROCESSING_CONTEXT_TASKRATELIMITER_HXX

#include "RateLimiter.hxx"

class RedisInterface;

class TaskRateLimiter
{
public:
    /**
     * @brief Test given id against an upper limit.
     * When the key is initially written into the backing store, a TTL value is
     * attached until end of day. When the TTL is over, the key is supposed to be
     * removed from the backing store. The RedisClient and MockRedisStore handle
     * this functionality, therefore nothing else has to be taken care of.
     *
     * @param redisIfc An initialized and usable redis interface.
     * @param hashedTelematikId The hashed telematik id, as hex string.
     * @return Returns true when the telematik id reached the upper limit, otherwise false.
     */
    static bool isBlockedForToday(RedisInterface& redisIfc, const std::string& hashedTelematikId);

    /**
     * @brief Update error counters for given id.
     * Like `isBlockedForToday`, a TTL is used here, too.
     *
     * @param redisIfc An initialized and usable redis interface.
     * @param hashedTelematikId The hashed telematik id, as hex string.
     */
    static void updateStatistic(RedisInterface& redisIfc, const std::string& hashedTelematikId);
};


#endif//ERP_PROCESSING_CONTEXT_TASKRATELIMITER_HXX
