// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
// non-exclusively licensed to gematik GmbH

#include "shared/ErpRequirements.hxx"
#include "exporter/util/RuntimeConfiguration.hxx"
#include "shared/util/Expect.hxx"

namespace exporter
{

bool RuntimeConfiguration::isPaused() const
{
    return mPause;
}

std::chrono::milliseconds RuntimeConfiguration::throttleValue() const
{
    return mThrottle;
}

void RuntimeConfiguration::pause()
{
    mPause = true;
}

void RuntimeConfiguration::resume()
{
    mPause = false;
}

void RuntimeConfiguration::throttle(const std::chrono::milliseconds& throttle)
{
    mThrottle = throttle;
}

RuntimeConfiguration::Getter::Getter(std::shared_ptr<const RuntimeConfiguration> runtimeConfiguration)
    : mRuntimeConfiguration(std::move(runtimeConfiguration))
    , mSharedLock(mRuntimeConfiguration->mSharedMutex)
{
}

bool RuntimeConfiguration::Getter::isPaused() const
{
    return mRuntimeConfiguration->isPaused();
}

std::chrono::milliseconds RuntimeConfiguration::Getter::throttleValue() const
{
    return mRuntimeConfiguration->throttleValue();
}


RuntimeConfiguration::Setter::Setter(std::shared_ptr<RuntimeConfiguration> runtimeConfiguration)
    : mRuntimeConfiguration(std::move(runtimeConfiguration))
    , mUniqueLock(mRuntimeConfiguration->mSharedMutex)
{
}

void RuntimeConfiguration::Setter::pause()
{
    mRuntimeConfiguration->pause();
}

void RuntimeConfiguration::Setter::resume()
{
    mRuntimeConfiguration->resume();
}

void RuntimeConfiguration::Setter::throttle(const std::chrono::milliseconds& throttle)
{
    mRuntimeConfiguration->throttle(throttle);
}

bool RuntimeConfiguration::Setter::isPaused() const
{
    return mRuntimeConfiguration->isPaused();
}

std::chrono::milliseconds RuntimeConfiguration::Setter::throttleValue() const
{
    return mRuntimeConfiguration->throttleValue();
}

}
