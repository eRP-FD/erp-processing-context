// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef ERP_EXPORTER_UTIL_RUNTIMECONFIGURATION_HXX
#define ERP_EXPORTER_UTIL_RUNTIMECONFIGURATION_HXX

#include "shared/util/RuntimeConfigurationBase.hxx"

#include <boost/core/noncopyable.hpp>
#include <mutex>
#include <shared_mutex>

namespace exporter
{
class RuntimeConfiguration : public shared::RuntimeConfigurationBase
{
public:
    static constexpr std::string_view parameter_pause = "Pause";
    static constexpr std::string_view parameter_epa = "epa";
    static constexpr std::string_view parameter_t_rezept = "t-rezept";
    static constexpr std::string_view parameter_resume = "Resume";
    static constexpr std::string_view parameter_throttle = "Throttle";

    class Getter;
    class Setter;

    enum class ThrottleMode
    {
        MANUAL,
        AUTOMATIC_EPA_LOOKUP
    };
    enum class ProcessorType
    {
        EPA,
        T_REZEPT
    };

    RuntimeConfiguration();

private:
    bool isPaused(ProcessorType processor) const;
    std::chrono::milliseconds throttleValue() const;
    ThrottleMode throttleMode() const;

    void pause(ProcessorType processor);
    void resume(ProcessorType processor);
    void throttle(ThrottleMode throttleMode, const std::chrono::milliseconds& throttle);

    void setMetricsLogThresholdMs(DurationCategory category, std::chrono::milliseconds thresholdMs);
    std::chrono::milliseconds getMetricsLogThresholdMs(DurationCategory category) const;
    std::map<DurationCategory, std::chrono::milliseconds> getMetricsLogThresholdsMs() const;

    mutable std::shared_mutex mSharedMutex;

    std::map<ProcessorType, bool> mPause;
    std::chrono::milliseconds mThrottle{0};
    ThrottleMode mThrottleMode{ThrottleMode::MANUAL};
};

class RuntimeConfiguration::Getter : boost::noncopyable
{
public:
    explicit Getter(std::shared_ptr<const RuntimeConfiguration> runtimeConfiguration);

    bool isPaused(ProcessorType processor) const;
    std::chrono::milliseconds throttleValue() const;
    ThrottleMode throttleMode() const;

    std::chrono::milliseconds getMetricsLogThresholdMs(DurationCategory category) const;
    std::map<DurationCategory, std::chrono::milliseconds> getMetricsLogThresholdsMs() const;

private:
    std::shared_ptr<const RuntimeConfiguration> mRuntimeConfiguration;
    std::shared_lock<std::shared_mutex> mSharedLock;
};

class RuntimeConfigurationGetter : public RuntimeConfiguration::Getter
{
public:
    using Getter::Getter;
};


class RuntimeConfiguration::Setter : boost::noncopyable
{
public:
    explicit Setter(std::shared_ptr<RuntimeConfiguration> runtimeConfiguration);

    void pause(ProcessorType processor);
    void resume(ProcessorType processor);
    void throttle(ThrottleMode throttleMode, const std::chrono::milliseconds& throttle);

    bool isPaused(ProcessorType processor) const;
    std::chrono::milliseconds throttleValue() const;
    ThrottleMode throttleMode() const;

    void setMetricsLogThresholdMs(DurationCategory category, std::chrono::milliseconds thresholdMs);

private:
    std::shared_ptr<RuntimeConfiguration> mRuntimeConfiguration;
    std::unique_lock<std::shared_mutex> mUniqueLock;
};

class RuntimeConfigurationSetter : public RuntimeConfiguration::Setter
{
public:
    using Setter::Setter;
};
}

#endif
