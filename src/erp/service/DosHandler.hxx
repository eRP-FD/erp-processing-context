/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_DOSHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_DOSHANDLER_HXX

#include "erp/database/redis/RateLimiter.hxx"

class DosHandler : public RateLimiter
{
public:
    explicit DosHandler(std::shared_ptr<RedisInterface> iface);


    void healthCheck() const;

    constexpr static std::string_view countField = "count";
    constexpr static std::string_view healthCheckKey = "health_check";
};

#endif
