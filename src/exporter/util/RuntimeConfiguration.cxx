// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "exporter/util/RuntimeConfiguration.hxx"
#include "shared/util/Configuration.hxx"
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

void RuntimeConfiguration::setMetricsLogThresholdMs(DurationCategory category, std::chrono::milliseconds thresholdMs)
{
    shared::RuntimeConfigurationBase::setMetricsLogThresholdMs(category, thresholdMs);
}

std::chrono::milliseconds RuntimeConfiguration::getMetricsLogThresholdMs(DurationCategory category) const
{
    return shared::RuntimeConfigurationBase::getMetricsLogThresholdMs(category);
}

std::map<DurationCategory, std::chrono::milliseconds> RuntimeConfiguration::getMetricsLogThresholdsMs() const
{
    return shared::RuntimeConfigurationBase::getMetricsLogThresholdsMs();
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

std::chrono::milliseconds RuntimeConfiguration::Getter::getMetricsLogThresholdMs(DurationCategory category) const
{
    return mRuntimeConfiguration->getMetricsLogThresholdMs(category);
}

std::map<DurationCategory, std::chrono::milliseconds> RuntimeConfiguration::Getter::getMetricsLogThresholdsMs() const
{
    return mRuntimeConfiguration->getMetricsLogThresholdsMs();
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

void RuntimeConfiguration::Setter::setMetricsLogThresholdMs(DurationCategory category,
                                                            std::chrono::milliseconds thresholdMs)
{
    mRuntimeConfiguration->setMetricsLogThresholdMs(category, thresholdMs);
}

}
