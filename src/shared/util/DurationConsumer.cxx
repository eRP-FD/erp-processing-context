/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/DurationConsumer.hxx"

#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/ExceptionHelper.hxx"


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
    DurationTimer::Receiver receiver = DurationConsumer::defaultReceiver;
    return DurationTimer(receiver, category, description, mSessionIdentifier.value_or("unknown"), keyValueMap);
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

void DurationConsumer::defaultReceiver(
    std::chrono::steady_clock::duration duration,
    const std::string &category,
    const std::string &description,
    const std::string &sessionIdentifier,
    const std::unordered_map<std::string, std::string> &keyValueMap,
    const std::optional<JsonLog::LogReceiver> &logReceiverOverride)
{
    const auto timingLoggingEnabled = Configuration::instance().timingLoggingEnabled(category);
    auto getLogReceiver = [logReceiverOverride, timingLoggingEnabled] {
        if (logReceiverOverride)
        {
            return *logReceiverOverride;
        }
        return timingLoggingEnabled ? JsonLog::makeInfoLogReceiver() : JsonLog::makeVLogReceiver(0);
    };
    const auto durationMusecs = std::chrono::duration_cast<std::chrono::microseconds>(duration);
    JsonLog log(LogId::INFO, getLogReceiver());
    log.keyValue("log-type", "timing")
        .keyValue("x-request-id", sessionIdentifier)
        .keyValue("category", category)
        .keyValue("description", description);
    for (const auto& item : keyValueMap)
    {
        log.keyValue(item.first, item.second);
    }
    log.keyValue("duration-us", gsl::narrow<size_t>(durationMusecs.count()));

    DurationTimer::Receiver receiver = getCurrent().mReceiver;
    if(receiver){
        receiver(duration, category, description, sessionIdentifier, keyValueMap, logReceiverOverride);
    }
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
