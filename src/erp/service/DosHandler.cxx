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

    const std::string key{ std::string{dosKeyPrefix} + sub};
    const auto tNow = time_point_cast<milliseconds>(system_clock::now());
    const auto tSpan = time_point<system_clock, milliseconds>( milliseconds( mTimespan ) );

    try
    {
        if (mInterface->exists(key))
        {
            auto countValue = mInterface->fieldValueForKey(key, countField).value_or(std::to_string(0));
            auto t0Value = mInterface->fieldValueForKey(key, t0Field).value_or(std::to_string(0));
            unsigned long count = std::stoul(countValue);
            const auto t0 = time_point<system_clock, milliseconds>( milliseconds( std::stoull(t0Value) ) );
            count++;
            if (mInterface->hasKeyWithField(key, std::string{blockedField}))
            {
                // Token is blocked.
                VLOG(1) << "Access token blocked and will expire in about " << (exp - tNow).count() << " ms." << std::endl;
                return false;
            }
            else if (count > mNumreqs && ((tNow - t0).count() < tSpan.time_since_epoch().count()))
            {
                // Too many attempts within timespan, mark token as blocked.
                mInterface->setKeyFieldValue(key, std::string{blockedField}, "1");

                VLOG(1) << count - 1 << " requests within t0 + " << (tNow - t0).count() << " ms. Blocked request attempt #" << count << std::endl;
                VLOG(1) << "Expires in about " << (exp - tNow).count() << " ms." << std::endl;
                return false;
            }

            // Token pass.
            mInterface->setKeyFieldValue(key, countField, std::to_string(count));
            VLOG(1) << "request #" << count << " within " << (tNow - t0).count() << " ms." << std::endl;
        }
        else
        {
            // Initial access, always pass.
            mInterface->setKeyFieldValue(key, countField, "1");
            mInterface->setKeyFieldValue(key, t0Field, std::to_string(tNow.time_since_epoch().count()));
            mInterface->setKeyExpireAt(key, exp);
            VLOG(1) << "request #" << 1 << " at t0 " << std::to_string(tNow.time_since_epoch().count()) << std::endl;
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
