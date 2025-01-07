/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */
#include "EventKvnr.hxx"
#include "shared/util/String.hxx"


model::EventKvnr::EventKvnr(const std::basic_string<std::byte>& kvnrHashed, std::optional<Timestamp> lastConsentCheck,
                            const std::optional<std::string>& assignedEpa, State state, std::int32_t retryCount)
    : mKvnrHashed(kvnrHashed)
    , mLastConsentCheck(lastConsentCheck)
    , mASsignedEpa(assignedEpa)
    , mState(state)
    , mRetryCount(retryCount)
{
}

const std::basic_string<std::byte>& model::EventKvnr::kvnrHashed() const
{
    return mKvnrHashed;
}
std::string model::EventKvnr::getLoggingId() const
{
    return String::toHexString(std::string(reinterpret_cast<const char*>(mKvnrHashed.data()), mKvnrHashed.size()));
}

std::optional<model::Timestamp> model::EventKvnr::getLastConsentCheck() const
{
    return mLastConsentCheck;
}

const std::optional<std::string>& model::EventKvnr::getASsignedEpa() const
{
    return mASsignedEpa;
}

model::EventKvnr::State model::EventKvnr::getState() const
{
    return mState;
}

std::int32_t model::EventKvnr::getRetryCount() const
{
    return mRetryCount;
}
