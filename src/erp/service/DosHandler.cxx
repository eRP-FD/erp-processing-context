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

DosHandler::DosHandler(std::unique_ptr<RedisInterface> iface) : mInterface(std::move(iface))
{
    mNumreqs = Configuration::instance().getOptionalIntValue(ConfigurationKey::TOKEN_ULIMIT_CALLS, 100);
    mTimespan = Configuration::instance().getOptionalIntValue(ConfigurationKey::TOKEN_ULIMIT_TIMESPAN_MS, 1000);
}

void DosHandler::setRequestsUpperLimit(size_t numreqs)
{
    mNumreqs = numreqs;
}

void DosHandler::setTimespan(uint64_t timespan)
{
    mTimespan = timespan;
}

bool DosHandler::updateAccessCounter(const std::string& sub, const std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>& exp) const
{
    using namespace std::chrono;

    const auto tNow = time_point_cast<milliseconds>(system_clock::now());
    const auto nowVal = tNow.time_since_epoch().count();
    const auto tSpan = time_point<system_clock, milliseconds>( milliseconds( mTimespan ) );
    const auto spanVal = tSpan.time_since_epoch().count();
    const auto tBucket = nowVal - (nowVal % spanVal);
    const auto tUpper = tBucket + spanVal;

    const std::string baseKey{ std::string{dosKeyPrefix} + sub };
    const std::string key{ baseKey + ":" + std::to_string(tBucket) };

    try
    {
        if (mInterface->exists(baseKey))
        {
            // Access token is blocked.
            return false;
        }

        auto calls = mInterface->incr(key);
        if (calls == 1)
        {
            // Initial access, always pass.
            mInterface->setKeyExpireAt(key, exp);
        }
        else if (calls > static_cast<int>(mNumreqs) && nowVal < tUpper)
        {
            // Token will be blocked from now on.
            VLOG(1) << "Access token blocked and will expire in about " << (exp - tNow).count() << " ms." << std::endl;
            mInterface->setKeyFieldValue(baseKey, "", "");
            mInterface->setKeyExpireAt(baseKey, exp);
            return false;
        }
        else
        {
            // Token pass.
            VLOG(1) << "request #" << calls << " within " << spanVal << " ms." << std::endl;
        }
        return true;
    }
    catch (const std::exception&)
    {
        JsonLog(LogId::DOS_ERROR)
            .message("DosHandler check failed due to previous errors, check skipped.");
        return true;
    }
    return false;
}

void DosHandler::healthCheck() const
{
    mInterface->setKeyFieldValue(healthCheckKey, countField, "1");
}
