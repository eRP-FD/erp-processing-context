/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */
#include "EventKvnr.hxx"
#include "shared/util/JsonLog.hxx"
#include "shared/util/String.hxx"

namespace model
{

EventKvnr::EventKvnr(const std::basic_string<std::byte>& kvnrHashed, std::optional<Timestamp> lastConsentCheck,
                            const std::optional<std::string>& assignedEpa, State state, std::int32_t retryCount)
    : mKvnrHashed(kvnrHashed)
    , mLastConsentCheck(lastConsentCheck)
    , mASsignedEpa(assignedEpa)
    , mState(state)
    , mRetryCount(retryCount)
{
}

const std::basic_string<std::byte>& EventKvnr::kvnrHashed() const
{
    return mKvnrHashed;
}
std::string EventKvnr::getLoggingId() const
{
    return String::toHexString(std::string(reinterpret_cast<const char*>(mKvnrHashed.data()), mKvnrHashed.size()));
}

std::optional<Timestamp> EventKvnr::getLastConsentCheck() const
{
    return mLastConsentCheck;
}

const std::optional<std::string>& EventKvnr::getASsignedEpa() const
{
    return mASsignedEpa;
}

EventKvnr::State EventKvnr::getState() const
{
    return mState;
}

std::int32_t EventKvnr::getRetryCount() const
{
    return mRetryCount;
}

JsonLog& operator<<(JsonLog& jsonLog, const EventKvnr& kvnr)
{
    return jsonLog.keyValue("kvnr", kvnr.getLoggingId());
}

JsonLog&& operator<<(JsonLog&& jsonLog, const EventKvnr& kvnr)
{
    jsonLog.keyValue("kvnr", kvnr.getLoggingId());
    return std::move(jsonLog);
}


}