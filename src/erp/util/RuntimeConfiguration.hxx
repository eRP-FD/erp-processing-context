// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include "shared/model/Timestamp.hxx"
#include "shared/util/DurationCategory.hxx"
#include "shared/util/RuntimeConfigurationBase.hxx"

#include <boost/core/noncopyable.hpp>
#include <gsl/gsl-lite.hpp>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <variant>

namespace erp
{
class RuntimeConfiguration : public shared::RuntimeConfigurationBase
{
public:
    static constexpr model::Timestamp::duration_t accept_pn3_max_active{std::chrono::hours{24}};
    static constexpr std::string_view parameter_accept_pn3 = "AcceptPN3";
    static constexpr std::string_view parameter_accept_pn3_expiry = "AcceptPN3Expiry";
    static constexpr std::string_view parameter_accept_pn3_max_active = "AcceptPN3MaxActive";

    class Getter;
    class Setter;

private:
    struct AcceptPN3Enabled {
        explicit AcceptPN3Enabled(const model::Timestamp& expiry);
        model::Timestamp acceptPN3Expiry;
    };
    struct AcceptPN3Disabled {
    };

    void enableAcceptPN3(const model::Timestamp& expiry);
    void disableAcceptPN3();

    bool isAcceptPN3Enabled() const;
    model::Timestamp getAcceptPN3Expiry() const;

    void setMetricsLogThresholdMs(DurationCategory category, std::chrono::milliseconds thresholdMs);
    std::chrono::milliseconds getMetricsLogThresholdMs(DurationCategory category) const;
    std::map<DurationCategory, std::chrono::milliseconds> getMetricsLogThresholdsMs() const;

    mutable std::shared_mutex mSharedMutex;

    std::variant<AcceptPN3Disabled, AcceptPN3Enabled> mAcceptPN3{AcceptPN3Disabled{}};
};

class RuntimeConfiguration::Getter : boost::noncopyable
{
public:
    explicit Getter(std::shared_ptr<const RuntimeConfiguration> runtimeConfiguration);

    bool isAcceptPN3Enabled() const;
    model::Timestamp getAcceptPN3Expiry() const;

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

    void enableAcceptPN3(const model::Timestamp& expiry);
    void disableAcceptPN3();

    bool isAcceptPN3Enabled() const;
    model::Timestamp getAcceptPN3Expiry() const;

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
