/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "Health.hxx"
#include "shared/erp-serverinfo.hxx"
#include "shared/model/RapidjsonDocument.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/Expect.hxx"
#include <rapidjson/document.h>

#include <unordered_map>
#include <string_view>

namespace model
{

namespace
{
const std::string health_template = R"--(
{
  "status": "DOWN",
  "checks": [],
  "version": {
    "release": "",
    "build": "",
    "buildType": "",
    "releasedate": ""
  }
}
)--";

constexpr std::string_view service_template_postgres = R"--(
{
  "name": "postgres",
  "status": "DOWN"
})--";

constexpr std::string_view service_template_eventdb = R"--(
{
  "name": "eventdb",
  "status": "DOWN"
})--";

constexpr std::string_view service_template_redis = R"--(
{
  "name": "redis",
  "status": "DOWN"
})--";

constexpr std::string_view service_template_hsm = R"--(
{
  "name": "hsm",
  "status": "DOWN",
  "data": {
    "ip": ""
  }
})--";

constexpr std::string_view service_template_tsl = R"--(
{
  "name": "TSL.xml",
  "status": "DOWN",
  "data": {
    "expiryDate": "",
    "sequenceNumber": "",
    "id": "",
    "hash": ""
  }
})--";

constexpr std::string_view service_template_bna = R"--(
{
  "name": "BNetzA.xml",
  "status": "DOWN",
  "data": {
    "expiryDate": "",
    "sequenceNumber": "",
    "id": "",
    "hash": ""
  }
})--";

constexpr std::string_view service_template_idp = R"--(
{
  "name": "IdpUpdater",
  "status": "DOWN"
})--";

constexpr std::string_view service_template_prngseed = R"--(
{
  "name": "SeedTimer",
  "status": "DOWN"
})--";

constexpr std::string_view service_template_teetoken = R"--(
{
  "name": "TeeTokenUpdater",
  "status": "DOWN"
})--";

constexpr std::string_view service_template_cfdsigerp = R"--(
{
  "name": "C.FD.SIG-eRP",
  "status": "DOWN",
  "data": {
    "timestamp": ""
  }
})--";

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
const rapidjson::Pointer currentTimestampPointer("/timestamp");
const rapidjson::Pointer healthCheckErrorPointer("/healthCheckError");
const rapidjson::Pointer connectionInfoPointer("/data/connection_info");
const rapidjson::Pointer tslExpiryPointer("/data/expiryDate");
const rapidjson::Pointer tslSequenceNumberPointer("/data/sequenceNumber");
const rapidjson::Pointer tslIdPointer("/data/id");
const rapidjson::Pointer tslHashPointer("/data/hash");

const std::string startupTimestamp = model::Timestamp::now().toXsDateTime();

RapidjsonNumberAsStringParserDocument<struct HealthMark>
stringToRapidjson(const std::string& source)
{
    RapidjsonNumberAsStringParserDocument<struct HealthMark> hT;
    rapidjson::StringStream ss(source.c_str());
    hT->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(ss);
    Expect3(! hT->HasParseError(), "error parsing json template for health model", std::logic_error);
    return hT;
}
}

Health::Health()
    : ResourceBase([]() {
        return stringToRapidjson(health_template);
    }().instance())
{
    setValue(startupPointer, startupTimestamp);
    setValue(currentTimestampPointer, model::Timestamp::now().toXsDateTime());
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

void Health::setEventDbStatus(const std::string_view& status, const std::string_view& connectionInfo,
                              std::optional<std::string_view> message)
{
    std::map<rapidjson::Pointer, std::string_view> data{{connectionInfoPointer, connectionInfo}};
    if (message)
    {
        data.emplace(rootCausePointer, *message);
    }
    setStatusInChecksArray("eventdb", status, data);
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

void Health::setTslStatus(std::string_view status, std::string_view expiry, std::string_view sequenceNumber,
                          std::string_view id, std::string_view hashValue, std::optional<std::string_view> message)
{
    std::map<rapidjson::Pointer, std::string_view> data{{tslExpiryPointer, expiry},
                                                        {tslSequenceNumberPointer, sequenceNumber},
                                                        {tslIdPointer, id},
                                                        {tslHashPointer, hashValue}};
    if (message)
    {
        data.emplace(rootCausePointer, *message);
    }
    setStatusInChecksArray(tsl, status, data);
}

void Health::setBnaStatus(std::string_view status, std::string_view expiry, std::string_view sequenceNumber,
                          std::string_view id, std::string_view hashValue, std::optional<std::string_view> message)
{
    std::map<rapidjson::Pointer, std::string_view> data{{tslExpiryPointer, expiry},
                                                        {tslSequenceNumberPointer, sequenceNumber},
                                                        {tslIdPointer, id},
                                                        {tslHashPointer, hashValue}};
    if (message)
    {
        data.emplace(rootCausePointer, *message);
    }
    setStatusInChecksArray(bna, status, data);
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
    std::unordered_map<std::string_view, std::string_view> templateMap = {
        {"postgres", service_template_postgres},
        {"eventdb", service_template_eventdb},
        {"redis", service_template_redis},
        {"hsm", service_template_hsm},
        {"TSL.xml", service_template_tsl},
        {"BNetzA.xml", service_template_bna},
        {"IdpUpdater", service_template_idp},
        {"SeedTimer", service_template_prngseed},
        {"TeeTokenUpdater", service_template_teetoken},
        {"C.FD.SIG-eRP", service_template_cfdsigerp},
    };
    if (arrayEntry == nullptr)
    {
        auto it = templateMap.find(name);
        if (it != templateMap.end()) {
            std::string_view source = it->second;
            auto tmpl = stringToRapidjson(std::string{source});
            auto v = copyValue(*tmpl);
            addToArray(checksPointer, std::move(v));
            arrayEntry = findMemberInArray(checksPointer, namePointer, name);
        }
    }
    Expect3(arrayEntry != nullptr, "did not find array entry " + std::string(name), std::logic_error);

    setKeyValue(*arrayEntry, statusPointer, status);
    for (const auto& item : data)
    {
        setKeyValue(*arrayEntry, item.first, item.second);
    }
}

}
