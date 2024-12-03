/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_HEALTH_HXX
#define ERP_PROCESSING_CONTEXT_HEALTH_HXX

#include "shared/model/ResourceBase.hxx"

#include <map>

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class Health : public ResourceBase
{
public:
    static constexpr std::string_view down = "DOWN";
    static constexpr std::string_view up = "UP";
    static constexpr std::string_view shutdown = "SHUTTING_DOWN";

    static constexpr std::string_view postgres = "postgres";
    static constexpr std::string_view redis = "redis";
    static constexpr std::string_view cFdSigErp = "C.FD.SIG-eRP";
    static constexpr std::string_view hsm = "hsm";
    static constexpr std::string_view tsl = "TSL.xml";
    static constexpr std::string_view bna = "BNetzA.xml";
    static constexpr std::string_view idp = "IdpUpdater";
    static constexpr std::string_view seedTimer = "SeedTimer";
    static constexpr std::string_view teeTokenUpdater = "TeeTokenUpdater";
    static constexpr std::string_view eventDb = "EventDb";

    Health();

    void setOverallStatus(const std::string_view& status);
    void setPostgresStatus(const std::string_view& status, const std::string_view& connectionInfo,
                           std::optional<std::string_view> message = std::nullopt);
    void setEventDbStatus(const std::string_view& status, const std::string_view& connectionInfo,
                          std::optional<std::string_view> message = std::nullopt);
    void setHsmStatus(const std::string_view& status, const std::string_view& device,
                      std::optional<std::string_view> message = std::nullopt);
    void setRedisStatus(const std::string_view& status, std::optional<std::string_view> message = std::nullopt);
    void setTslStatus(std::string_view status, std::string_view expiry, std::string_view sequenceNumber,
                      std::string_view id, std::string_view hashValue,
                      std::optional<std::string_view> message = std::nullopt);
    void setBnaStatus(std::string_view status, std::string_view expiry, std::string_view sequenceNumber,
                      std::string_view id, std::string_view hashValue,
                      std::optional<std::string_view> message = std::nullopt);
    void setCFdSigErpStatus(const std::string_view& status, const std::string_view& timestamp,
                            const std::string_view& policy, const std::string_view& expiry,
                            std::optional<std::string_view> message = std::nullopt);
    void setIdpStatus(const std::string_view& status, std::optional<std::string_view> message = std::nullopt);
    void setSeedTimerStatus(const std::string_view& status, std::optional<std::string_view> message = std::nullopt);
    void setTeeTokenUpdaterStatus(const std::string_view& status, std::optional<std::string_view> message = std::nullopt);
    // the health check itself had an unexpected error:
    void setHealthCheckError(const std::string_view& errorMessage);

private:
    void setStatusInChecksArray(const std::string_view& name, const std::string_view& status, const rapidjson::Pointer& messagePointer, std::optional<std::string_view> message);
    void setStatusInChecksArray(const std::string_view& name, const std::string_view& status,
                                const std::map<rapidjson::Pointer, std::string_view>& data);
};

}

#endif//ERP_PROCESSING_CONTEXT_HEALTH_HXX
