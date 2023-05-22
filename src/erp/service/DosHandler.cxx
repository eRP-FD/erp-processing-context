/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/DosHandler.hxx"
#include "erp/service/RedisInterface.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/JsonLog.hxx"
#include "erp/util/TLog.hxx"

#include <chrono>
#include <string>

DosHandler::DosHandler(std::shared_ptr<RedisInterface> iface)
    : RateLimiter(std::move(iface), "ERP-PC-DOS",
                  gsl::narrow<size_t>(Configuration::instance().getOptionalIntValue(ConfigurationKey::TOKEN_ULIMIT_CALLS, 100)),
                  std::chrono::milliseconds(Configuration::instance().getOptionalIntValue(ConfigurationKey::TOKEN_ULIMIT_TIMESPAN_MS, 1000)))
{
}

void DosHandler::healthCheck() const
{
    mInterface->setKeyFieldValue(healthCheckKey, countField, "1");
}
