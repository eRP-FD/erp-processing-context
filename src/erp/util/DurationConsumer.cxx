/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/DurationConsumer.hxx"

#include "erp/util/Expect.hxx"
#include "erp/util/ExceptionHelper.hxx"


DurationTimer::DurationTimer (
    Receiver& receiver,
    const std::string& description,
    const std::string& sessionIdentifier)
    : mReceiver(receiver),
      mDescription(description),
      mSessionIdentifier(sessionIdentifier),
      mStart(std::chrono::system_clock::now())
{
}


DurationTimer::~DurationTimer (void)
{
    if (std::uncaught_exceptions() > 0)
    {
        // Sadly, we can not use the ExceptionHelper to get detailed information about the exception because we can
        // not rethrow it.
        notifyFailure("uncaught exception");
    }
    else
    {
        const auto end = std::chrono::system_clock::now();
        if (mStart.has_value() && mReceiver)
            mReceiver(end-mStart.value(), mDescription + " was successful", mSessionIdentifier);
    }
}


void DurationTimer::notifyFailure(const std::string& description)
{
    const auto end = std::chrono::system_clock::now();
    if (mStart.has_value() && mReceiver)
    {
        mReceiver(end-mStart.value(), mDescription + " failed: " + description, mSessionIdentifier);
        mStart = std::nullopt;
    }
}


DurationTimer DurationConsumer::getTimer (const std::string& description)
{
    return DurationTimer(mReceiver, description, mSessionIdentifier.value_or("unknown"));
}

DurationTimer DurationConsumer::getTimer(const std::string& description, DurationTimer::Receiver& receiver) const
{
    return DurationTimer(receiver, description, mSessionIdentifier.value_or("unknown"));
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


void DurationConsumer::reset (void)
{
    Expect(mIsInitialized, "not initialized");

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
