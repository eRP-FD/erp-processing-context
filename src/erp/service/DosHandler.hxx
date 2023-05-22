/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
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
