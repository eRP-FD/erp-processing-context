/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Health.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/RapidjsonDocument.hxx"

#include <rapidjson/document.h>

namespace model
{

namespace
{
constexpr std::string_view health_template = R"--(
{
  "status": "DOWN",
  "checks": [
    {
      "name": "postgres",
      "status": "DOWN"
    },
    {
      "name": "redis",
      "status": "DOWN"
    },
    {
      "name": "hsm",
      "status": "DOWN",
      "data": {
        "ip": ""
      }
    },
    {
      "name": "TSL.xml",
      "status": "DOWN"
    },
    {
      "name": "BNetzA.xml",
      "status": "DOWN"
    },
    {
      "name": "IdpUpdater",
      "status": "DOWN"
    },
    {
      "name": "SeedTimer",
      "status": "DOWN"
    },
    {
      "name": "TeeTokenUpdater",
      "status": "DOWN"
    },
    {
      "name": "C.FD.SIG-eRP",
      "status": "DOWN",
      "data": {
        "timestamp": ""
      }
    }
  ],
  "version": {
    "release": "",
    "build": "",
    "buildType": "",
    "releasedate": ""
  }
}
)--";

const rapidjson::Pointer statusPointer("/status");
const rapidjson::Pointer namePointer("/name");
const rapidjson::Pointer checksPointer("/checks");
const rapidjson::Pointer rootCausePointer("/data/rootCause");
const rapidjson::Pointer ipPointer("/data/ip");
const rapidjson::Pointer timestampPointer("/data/timestamp");
const rapidjson::Pointer policyPointer("/data/policy");
const rapidjson::Pointer expiryPointer("/data/certificate_expiry");
const rapidjson::Pointer buildPointer("/version/build");
const rapidjson::Pointer buildTypePointer("/version/buildType");
const rapidjson::Pointer releasePointer("/version/release");
const rapidjson::Pointer releasedatePointer("/version/releasedate");
const rapidjson::Pointer startupPointer("/startup");
const rapidjson::Pointer healthCheckErrorPointer("/healthCheckError");
const rapidjson::Pointer connectionInfoPointer("/data/connection_info");
const std::string startupTimestamp = model::Timestamp::now().toXsDateTime();
}

Health::Health()
    : ResourceBase(ResourceBase::NoProfile,
                   []() {
                       RapidjsonNumberAsStringParserDocument<struct HealthMark> hT;
                       rapidjson::StringStream ss(health_template.data());
                       hT->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(ss);
                       Expect3(! hT->HasParseError(), "error parsing json template for health model", std::logic_error);
                       return hT;
                   }()
                       .instance())
{
    setValue(startupPointer, startupTimestamp);
    setValue(buildPointer, ErpServerInfo::BuildVersion());
    setValue(buildTypePointer, ErpServerInfo::BuildType());
    setValue(releasePointer, ErpServerInfo::ReleaseVersion());
    setValue(releasedatePointer, ErpServerInfo::ReleaseDate());
}

void Health::setOverallStatus(const std::string_view& status)
{
    setValue(statusPointer, status);
}

void Health::setPostgresStatus(const std::string_view& status, const std::string_view& connectionInfo,
                               std::optional<std::string_view> message)
{
    std::map<rapidjson::Pointer, std::string_view> data{{connectionInfoPointer, connectionInfo}};
    if (message)
    {
        data.emplace(rootCausePointer, *message);
    }
    setStatusInChecksArray("postgres", status, data);
}

void Health::setHsmStatus(const std::string_view& status, const std::string_view& device,
                          std::optional<std::string_view> message)
{
    std::map<rapidjson::Pointer, std::string_view> data{{ipPointer, device}};
    if (message)
    {
        data.emplace(rootCausePointer, *message);
    }
    setStatusInChecksArray(hsm, status, data);
}

void Health::setRedisStatus(const std::string_view& status, std::optional<std::string_view> message)
{
    setStatusInChecksArray(redis, status, rootCausePointer, message);
}

void Health::setTslStatus(const std::string_view& status, std::optional<std::string_view> message)
{
    setStatusInChecksArray(tsl, status, rootCausePointer, message);
}

void Health::setBnaStatus(const std::string_view& status, std::optional<std::string_view> message)
{
    setStatusInChecksArray(bna, status, rootCausePointer, message);
}

void Health::setCFdSigErpStatus(const std::string_view& status, const std::string_view& timestamp,
                                const std::string_view& policy, const std::string_view& expiry,
                                std::optional<std::string_view> message)
{
    std::map<rapidjson::Pointer, std::string_view> data{{timestampPointer, timestamp},
                                                        {policyPointer, policy}, {expiryPointer, expiry}};
    if (message)
    {
        data.emplace(rootCausePointer, *message);
    }
    setStatusInChecksArray(cFdSigErp, status, data);
}

void Health::setIdpStatus(const std::string_view& status, std::optional<std::string_view> message)
{
    setStatusInChecksArray(idp, status, rootCausePointer, message);
}

void Health::setSeedTimerStatus(const std::string_view& status, std::optional<std::string_view> message)
{
    setStatusInChecksArray(seedTimer, status, rootCausePointer, message);
}

void Health::setTeeTokenUpdaterStatus(const std::string_view& status, std::optional<std::string_view> message)
{
    setStatusInChecksArray(teeTokenUpdater, status, rootCausePointer, message);
}

void Health::setHealthCheckError(const std::string_view& errorMessage)
{
    setValue(healthCheckErrorPointer, errorMessage);
}

void Health::setStatusInChecksArray(const std::string_view& name, const std::string_view& status,
                                    const rapidjson::Pointer& messagePointer, std::optional<std::string_view> message)
{
    if (message)
    {
        setStatusInChecksArray(name, status, {{messagePointer, *message}});
    }
    else
    {
        setStatusInChecksArray(name, status, {});
    }
}

void Health::setStatusInChecksArray(const std::string_view& name, const std::string_view& status,
    const std::map<rapidjson::Pointer, std::string_view>& data)
{
    auto* arrayEntry = findMemberInArray(checksPointer, namePointer, name);
    Expect3(arrayEntry != nullptr, "did not find array entry " + std::string(name), std::logic_error);
    setKeyValue(*arrayEntry, statusPointer, status);
    for (const auto& item : data)
    {
        setKeyValue(*arrayEntry, item.first, item.second);
    }
}

}
