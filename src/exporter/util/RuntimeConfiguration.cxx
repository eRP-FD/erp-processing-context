// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "exporter/util/RuntimeConfiguration.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"

namespace exporter
{

RuntimeConfiguration::RuntimeConfiguration()
{
    for (const auto processor : magic_enum::enum_values<ProcessorType>())
    {
        mPause[processor] = false;
    }
}

bool RuntimeConfiguration::isPaused(const ProcessorType processor) const
{
    return mPause.at(processor);
}

std::chrono::milliseconds RuntimeConfiguration::throttleValue() const
{
    return mThrottle;
}

RuntimeConfiguration::ThrottleMode RuntimeConfiguration::throttleMode() const
{
    return mThrottleMode;
}

void RuntimeConfiguration::pause(const ProcessorType processor)
{
    mPause[processor] = true;
}

void RuntimeConfiguration::resume(const ProcessorType processor)
{
    mPause[processor] = false;
}

void RuntimeConfiguration::throttle(ThrottleMode throttleMode, const std::chrono::milliseconds& throttle)
{
    mThrottle = throttle;
    mThrottleMode = throttleMode;
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

bool RuntimeConfiguration::Getter::isPaused(const ProcessorType processor) const
{
    return mRuntimeConfiguration->isPaused(processor);
}

std::chrono::milliseconds RuntimeConfiguration::Getter::throttleValue() const
{
    return mRuntimeConfiguration->throttleValue();
}

RuntimeConfiguration::ThrottleMode RuntimeConfiguration::Getter::throttleMode() const
{
    return mRuntimeConfiguration->throttleMode();
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

void RuntimeConfiguration::Setter::pause(const ProcessorType processor)
{
    mRuntimeConfiguration->pause(processor);
}

void RuntimeConfiguration::Setter::resume(const ProcessorType processor)
{
    mRuntimeConfiguration->resume(processor);
}

void RuntimeConfiguration::Setter::throttle(ThrottleMode throttleMode, const std::chrono::milliseconds& throttle)
{
    mRuntimeConfiguration->throttle(throttleMode, throttle);
}

bool RuntimeConfiguration::Setter::isPaused(const ProcessorType processor) const
{
    return mRuntimeConfiguration->isPaused(processor);
}

std::chrono::milliseconds RuntimeConfiguration::Setter::throttleValue() const
{
    return mRuntimeConfiguration->throttleValue();
}

RuntimeConfiguration::ThrottleMode RuntimeConfiguration::Setter::throttleMode() const
{
    return mRuntimeConfiguration->throttleMode();
}

void RuntimeConfiguration::Setter::setMetricsLogThresholdMs(DurationCategory category,
                                                            std::chrono::milliseconds thresholdMs)
{
    mRuntimeConfiguration->setMetricsLogThresholdMs(category, thresholdMs);
}

}
