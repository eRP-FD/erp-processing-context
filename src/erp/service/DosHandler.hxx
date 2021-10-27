/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_DOSHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_DOSHANDLER_HXX

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string_view>

#include "erp/service/RedisInterface.hxx"

class DosHandler
{
public:
    explicit DosHandler(std::unique_ptr<RedisInterface> iface);


    void setRequestsUpperLimit(size_t numreqs);


    void setTimespan(uint64_t timespan);


    /**
         * Add item into the storage if it's not yet there, otherwise
         * update the item.
         *
         * Return true if token may be further processed and false when token is marked as blocked.
         */
    bool updateAccessCounter(const std::string& sub,
                             const std::chrono::time_point<std::chrono::system_clock,
                             std::chrono::milliseconds>& exp) const;

    void healthCheck() const;

    constexpr static std::string_view dosKeyPrefix = "ERP-DOS:";
    constexpr static std::string_view blockedField = "blocked";
    constexpr static std::string_view countField = "count";
    constexpr static std::string_view t0Field = "t0";
    constexpr static std::string_view healthCheckKey = "health_check";

private:
    std::unique_ptr<RedisInterface> mInterface;
    size_t mNumreqs{0};
    uint64_t mTimespan{0};
};

#endif
