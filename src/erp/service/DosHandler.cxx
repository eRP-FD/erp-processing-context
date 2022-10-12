/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/DosHandler.hxx"
#include "erp/service/RedisInterface.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/JsonLog.hxx"
#include "erp/util/TLog.hxx"

#include <chrono>
#include <string>

DosHandler::DosHandler(std::shared_ptr<RedisInterface> iface)
    : RateLimiter(iface, "ERP-PC-DOS",
                  Configuration::instance().getOptionalIntValue(ConfigurationKey::TOKEN_ULIMIT_CALLS, 100),
                  Configuration::instance().getOptionalIntValue(ConfigurationKey::TOKEN_ULIMIT_TIMESPAN_MS, 1000))
{
}

void DosHandler::healthCheck() const
{
    mInterface->setKeyFieldValue(healthCheckKey, countField, "1");
}
