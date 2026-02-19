/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "BdeMessage.hxx"
#include "shared/util/JsonLog.hxx"

#include <unordered_map>

#include "shared/util/Expect.hxx"

namespace
{
const std::unordered_map<std::string, std::string> request_usecase_mapping =
    // clang-format off
    {
        {
            "/epa/medication/api/v1/fhir/$provide-prescription-erp", "ERP.UC_5_1"
        },
        {
            "/epa/medication/api/v1/fhir/$cancel-prescription-erp", "ERP.UC_5_2"
        },
        {
            "/epa/medication/api/v1/fhir/$provide-dispensation-erp", "ERP.UC_5_3"
        },
        {
            "/epa/medication/api/v1/fhir/$cancel-dispensation-erp", "ERP.UC_5_4"
        },
        {
            "/information/api/v1/ehr/consentdecisions", "ERP.UC_5_5"
        },
        {
            "/epa/authz/v1/send_authorization_request_bearertoken", "ERP.UC_5_6"
        },
        {
            "/ords/rezepte/oauth/token", "ERP.UC_5_7"
        },
        {
            "/ords/rezepte/t-rezept/v1", "ERP.UC_5_8"
        },
        {
            "/fdv/search/HealthcareService", "ERP.UC_5_9"
        },
    };
// clang-format on
}

void BDEMessage::Data::merge(const BDEMessage::Data& data)
{
    if (data.startTime)
    {
        startTime = data.startTime.value();
    }
    if (data.endTime)
    {
        endTime = data.endTime.value();
    }
    if (data.lastModified)
    {
        lastModified = data.lastModified.value();
    }
    if (data.hashedKvnr)
    {
        hashedKvnr = data.hashedKvnr.value();
    }
    if (data.cid)
    {
        cid = data.cid.value();
    }
    if (data.errorCode)
    {
        errorCode = data.errorCode.value();
    }
    if (data.innerResponseCode)
    {
        innerResponseCode = data.innerResponseCode;
    }
    if (data.error)
    {
        error = data.error.value();
    }
    if (data.host)
    {
        host = data.host.value();
    }
    if (data.innerOperation)
    {
        innerOperation = data.innerOperation;
    }
    if (data.ip)
    {
        ip = data.ip.value();
    }
    if (data.prescriptionId)
    {
        prescriptionId = data.prescriptionId;
    }
    if (data.processor)
    {
        processor = data.processor;
    }
    if (data.requestId)
    {
        requestId = data.requestId;
    }
    if (data.responseCode)
    {
        responseCode = data.responseCode;
    }
}

BDEMessage::BDEMessage(Data data)
    : mData(std::move(data))
{
}

BDEMessage::~BDEMessage()
{
    publish();
}

void BDEMessage::update(const BDEMessage::Data& data)
{
    mData.merge(data);
}

const BDEMessage::Data& BDEMessage::getData() const
{
    return mData;
}

void BDEMessage::publish()
{
    const auto now = model::Timestamp::now();

    const model::Timestamp startTime = mData.startTime.value_or(now);
    const model::Timestamp endTime = mData.endTime.value_or(now);
    const model::Timestamp lastModified = mData.lastModified.value_or(now);

    JsonLog log(LogId::INFO, JsonLog::makeInfoLogReceiver(), false);

    auto logIfNotEmpty = [&](std::string_view key, const std::optional<std::string>& v) {
        if (v && ! v->empty())
        {
            log.keyValue(key, *v);
        }
    };

    auto logIfPresent = [&](std::string_view key, const auto& opt, auto transform) {
        if (opt)
        {
            log.keyValue(key, transform(*opt));
        }
    };

    auto toMsSinceEpoch = [](const model::Timestamp& ts) -> int64_t {
        return std::chrono::duration_cast<std::chrono::milliseconds>(ts.toChronoTimePoint().time_since_epoch()).count();
    };

    auto removePrefix = [](const std::string& s) -> std::string {
        static constexpr std::array<std::string_view, 2> prefixes{"MEDICATIONSVC_", "MEDSVC_"};
        for (auto p : prefixes)
        {
            if (s.starts_with(p))
            {
                std::string result = s;
                result.erase(0, p.size());
                return result;
            }
        }
        return s;
    };

    log.keyValue("log_type", BDEMessage::log_type);

    log.keyValue("timestamp", startTime.toXsDateTime());
    log.keyValue("x_request_id", mData.requestId.value_or(""));

    const auto innerOp = mData.innerOperation.value_or("");
    log.keyValue("request_operation", innerOp);

    if (const auto it = request_usecase_mapping.find(innerOp); it != request_usecase_mapping.end())
    {
        log.keyValue("operation", it->second);
    }
    else
    {
        log.keyValue("operation", "");
    }

    logIfNotEmpty("endpoint_host", mData.host);
    logIfNotEmpty("endpoint_ip", mData.ip);

    log.keyValue("response_code", static_cast<size_t>(mData.responseCode.value_or(0)));

    const auto startMs = toMsSinceEpoch(startTime);
    const auto endMs = toMsSinceEpoch(endTime);
    const auto modMs = toMsSinceEpoch(lastModified);

    log.keyValue("response_time", static_cast<size_t>(endMs - startMs));

    // lastModified comes from the event and predates request processing.
    // If unset, the resulting value is 0, which is acceptable for logging.
    if (startMs < modMs)
    {
        // Log the data mismatch since this is usually a programming error.
        TVLOG(1) << "bde data mismatch, startTime is less than lastModified.";
        log.keyValue("duration_in_ms", static_cast<size_t>(0));
    }
    else
    {
        log.keyValue("duration_in_ms", static_cast<size_t>(startMs - modMs));
    }

    log.keyValue("error", mData.error.value_or(""));

    logIfNotEmpty("prescription_id", mData.prescriptionId);
    logIfNotEmpty("cid", mData.cid);

    logIfPresent("inner_response_code", mData.innerResponseCode, [](auto v) {
        return static_cast<size_t>(v);
    });

    if (mData.hashedKvnr)
    {
        log << *mData.hashedKvnr;
    }

    if (mData.errorCode)
    {
        log.keyValue("error_component", removePrefix(*mData.errorCode));
    }

    if (mData.processor)
    {
        log.keyValue("processor", *mData.processor);
    }
}
