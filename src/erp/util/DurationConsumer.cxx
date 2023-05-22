/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/DurationConsumer.hxx"

#include "erp/util/Expect.hxx"
#include "erp/util/ExceptionHelper.hxx"


DurationTimer::DurationTimer (
    Receiver& receiver,
    const std::string& category,
    const std::string& description,
    const std::string& sessionIdentifier,
    const std::unordered_map<std::string, std::string>& keyValueMap)
    : mReceiver(receiver),
      mCategory(category),
      mDescription(description),
      mSessionIdentifier(sessionIdentifier),
      mKeyValueMap(keyValueMap),
      mStart(std::chrono::steady_clock::now())
{
}


DurationTimer::~DurationTimer (void)
{
    const auto end = std::chrono::steady_clock::now();
    std::string description = mDescription;
    std::optional<JsonLog::LogReceiver> logReceiver;
    if (std::uncaught_exceptions() > mUncaughtExceptions)
    {
        // Sadly, we can not use the ExceptionHelper to get detailed information about the exception because we can
        // not rethrow it.
        description.append(" failed due to uncaught exception");
        logReceiver.emplace(JsonLog::makeWarningLogReceiver());
    }
    else
    {
        description.append(" was successful");
    }
    if (mStart.has_value() && mReceiver)
    {
        mReceiver(end - mStart.value(), mCategory, description, mSessionIdentifier, mKeyValueMap, logReceiver);
    }
}


void DurationTimer::notifyFailure(const std::string& description)
{
    const auto end = std::chrono::steady_clock::now();
    if (mStart.has_value() && mReceiver)
    {
        mReceiver(end - mStart.value(), mCategory, mDescription + " failed: " + description, mSessionIdentifier,
                  mKeyValueMap, JsonLog::makeErrorLogReceiver());
        mStart = std::nullopt;
    }
}

void DurationTimer::keyValue(const std::string& key, const std::string& value)
{
    mKeyValueMap[key] = value;
}


DurationTimer DurationConsumer::getTimer(const std::string& category, const std::string& description,
                                         const std::unordered_map<std::string, std::string>& keyValueMap)
{
    return DurationTimer(mReceiver, category, description, mSessionIdentifier.value_or("unknown"), keyValueMap);
}

DurationConsumer& DurationConsumer::getCurrent (void)
{
#ifndef _WIN32
    thread_local static DurationConsumer mDurationConsumer;
#else
    // For windows there is only a single duration consumer, which is never activated and which acts as a /dev/null device.
    static DurationConsumer mDurationConsumer;
#endif
    return mDurationConsumer;
}


void DurationConsumer::initialize (
    const std::string& sessionIdentifier,
    DurationTimer::Receiver&& receiver)

{
    Expect( ! mIsInitialized, "already initialized");

    mIsInitialized = true;
    mSessionIdentifier = sessionIdentifier;
    mReceiver = std::move(receiver);
}


void DurationConsumer::reset (void) noexcept
{
    mIsInitialized = false;
    mSessionIdentifier = std::nullopt;
    mReceiver = {};
}


bool DurationConsumer::isInitialized (void) const
{
    return mIsInitialized;
}


std::optional<std::string> DurationConsumer::getSessionIdentifier (void) const
{
    return mSessionIdentifier;
}


DurationConsumerGuard::DurationConsumerGuard (
    const std::string& sessionIdentifier,
    DurationTimer::Receiver&& receiver)
{
    DurationConsumer::getCurrent().initialize(sessionIdentifier, std::move(receiver));
}


DurationConsumerGuard::~DurationConsumerGuard (void)
{
    DurationConsumer::getCurrent().reset();
}
