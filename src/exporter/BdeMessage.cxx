/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "BdeMessage.hxx"
#include "shared/util/JsonLog.hxx"

namespace
{
std::unordered_map<std::string, std::string> request_usecase_mapping =
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
    };
// clang-format on
}

BDEMessage::BDEMessage()
    : mStartTime{model::Timestamp::now()}
    , mRequestId{}
    , mInnerOperation{}
    , mPrescriptionId{}
    , mHost{}
    , mIp{}
    , mCid{}
    , mInnerResponseCode{}
    , mResponseCode{}
    , mEndTime{model::Timestamp::now()}
    , mLastModified{model::Timestamp::now()}
    , mError{}
{
}

BDEMessage::~BDEMessage()
{
	publish();
}

void BDEMessage::publish()
{
    JsonLog log(LogId::INFO, JsonLog::makeInfoLogReceiver(), false);
    log.keyValue("log_type", BDEMessage::log_type);
    log.keyValue("timestamp", mStartTime.toXsDateTime());
    log.keyValue("x_request_id", mRequestId);
    log.keyValue("operation", request_usecase_mapping.find(mInnerOperation) == request_usecase_mapping.end()
                                  ? ""
                                  : request_usecase_mapping[mInnerOperation]);
    log.keyValue("request_operation", mInnerOperation);
    if (!mPrescriptionId.empty())
    {
        log.keyValue("prescription_id", mPrescriptionId);
    }
    log.keyValue("endpoint_host", mHost);
    log.keyValue("endpoint_ip", mIp);
    if (mCid)
    {
        log.keyValue("cid", mCid.value());
    }
    if (mInnerResponseCode)
    {
        log.keyValue("inner_response_code", std::to_string(mInnerResponseCode.value()));
    }
    log.keyValue("response_code", std::to_string(mResponseCode));

    const auto start = std::chrono::duration_cast<std::chrono::milliseconds>(mStartTime.toChronoTimePoint().time_since_epoch()).count();
    const auto end = std::chrono::duration_cast<std::chrono::milliseconds>(mEndTime.toChronoTimePoint().time_since_epoch()).count();
    const auto lastModified  = std::chrono::duration_cast<std::chrono::milliseconds>(mLastModified.toChronoTimePoint().time_since_epoch()).count();
    log.keyValue("response_time", std::to_string(end - start));
    log.keyValue("duration_in_ms", std::to_string(start - lastModified));

    log.keyValue("error", mError);
}