/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */
#include "EventKvnr.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "shared/util/JsonLog.hxx"
#include "shared/util/String.hxx"

namespace model
{

namespace
{

template<typename Duration>
bool isOlderThan(const Timestamp::timepoint_t& timestamp, typename Duration::rep dur)
{
    auto now = std::chrono::system_clock::now();
    auto duration = now - timestamp;
    auto rep_duration = std::chrono::duration_cast<Duration>(duration);
    return rep_duration.count() >= dur;
}

}

EventKvnr::EventKvnr(const std::basic_string<std::byte>& kvnrHashed, std::optional<Timestamp> lastConsentCheck,
                            const std::optional<std::string>& assignedEpa, State state, std::int32_t retryCount)
    : mKvnrHashed(kvnrHashed)
    , mLastConsentCheck(lastConsentCheck)
    , mAssignedEpa(assignedEpa)
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

const std::optional<std::string>& EventKvnr::getAssignedEpa() const
{
    return mAssignedEpa;
}

EventKvnr::State EventKvnr::getState() const
{
    return mState;
}

std::int32_t EventKvnr::getRetryCount() const
{
    return mRetryCount;
}

bool EventKvnr::useCachedValues() const
{
    A_25940.start("Do not used a cached entry if older than 180 days.");
    return (getAssignedEpa().has_value() && not getAssignedEpa()->empty() && getLastConsentCheck().has_value() &&
            not isOlderThan<std::chrono::days>(getLastConsentCheck().value().toChronoTimePoint(), 180));
    A_25940.finish();
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
