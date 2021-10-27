/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/registration/RegistrationManager.hxx"

#include <chrono>

#include "erp/database/RedisClient.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"

using namespace std::chrono;

namespace {

constexpr std::string_view teeInstanceKey = "ERP_VAU_INSTANCE";
constexpr std::string_view eventFieldName = "event";
constexpr std::string_view timestampFieldName =  "timestamp";

}


//static
std::unique_ptr<RegistrationInterface>
RegistrationManager::createFromConfig(
    const std::uint16_t aTeePort,
    const Configuration& configuration)
{
    return std::make_unique<RegistrationManager>(
        configuration.serverHost(),
        aTeePort,
        std::make_unique<RedisClient>());
}


RegistrationManager::RegistrationManager(
    const std::string& aTeeHost,
    const std::uint16_t aTeePort,
    std::unique_ptr<RedisInterface> aRedisInterface)
    : mRedisKey(std::string(teeInstanceKey) + ':' + aTeeHost + ':' + std::to_string(aTeePort))
    , mRedisInterface(std::move(aRedisInterface))
{
    TLOG(WARNING) << "Initializing Registration Manager: host=" << aTeeHost << ", port=" << aTeePort;
}


RegistrationManager::RegistrationManager(RegistrationManager&& other) noexcept
    : mRedisKey(other.mRedisKey)
    , mRedisInterface(std::move(other.mRedisInterface))
{
}


void RegistrationManager::registration()
{
    TVLOG(1) << "Registration of TEE instance." << std::endl;

    const auto now = time_point_cast<seconds>(system_clock::now());
    mRedisInterface->setKeyFieldValue(mRedisKey, eventFieldName, "startup");
    mRedisInterface->setKeyFieldValue(mRedisKey, timestampFieldName, std::to_string(now.time_since_epoch().count()));

    TVLOG(1) << "Registration TEE instance successful." << std::endl;
}


void RegistrationManager::deregistration()
{
    TVLOG(1) << "Deregistration of TEE instance." << std::endl;

    const auto now = time_point_cast<seconds>(system_clock::now());
    mRedisInterface->setKeyFieldValue(mRedisKey, eventFieldName, "unregister");
    mRedisInterface->setKeyFieldValue(mRedisKey, timestampFieldName, std::to_string(now.time_since_epoch().count()));

    TVLOG(1) << "Deregistration of TEE instance successful." << std::endl;
}


void RegistrationManager::heartbeat()
{
    TVLOG(3) << "Heartbeat from TEE instance." << std::endl;

    const auto now = time_point_cast<seconds>(system_clock::now());
    mRedisInterface->setKeyFieldValue(mRedisKey, eventFieldName, "heartbeat");
    mRedisInterface->setKeyFieldValue(mRedisKey, timestampFieldName, std::to_string(now.time_since_epoch().count()));

    TVLOG(1) << "Heartbeat from TEE instance successful." << std::endl;
}

