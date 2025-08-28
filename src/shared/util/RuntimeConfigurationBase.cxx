// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "shared/util/Configuration.hxx"
#include "shared/util/RuntimeConfigurationBase.hxx"
#include "shared/util/TLog.hxx"

#include <magic_enum/magic_enum.hpp>

namespace shared
{

const std::map<std::string, DurationCategory> RuntimeConfigurationBase::parameter_metrics_category_log_threshold_ms =
    [] {
        std::map<std::string, DurationCategory> parameters;
        for (const auto& durationCategory : magic_enum::enum_values<DurationCategory>())
        {
            parameters.emplace(std::string{magic_enum::enum_name(durationCategory)}.append("MetricLogThresholdMs"),
                               durationCategory);
        }
        return parameters;
    }();

std::map<DurationCategory, std::chrono::milliseconds> RuntimeConfigurationBase::defaultMetricsLogThresholdsMs()
{
    using namespace std::chrono_literals;
    std::map<DurationCategory, std::chrono::milliseconds> defaults;
    for (const auto& durationCategory : magic_enum::enum_values<DurationCategory>())
    {
        switch (durationCategory)
        {
            case DurationCategory::redis:
            case DurationCategory::httpclient:
                defaults[durationCategory] = 10ms;
                break;
            case DurationCategory::postgres:
                defaults[durationCategory] = 60ms;
                break;
            case DurationCategory::fhirvalidation:
            case DurationCategory::ocsprequest:
                defaults[durationCategory] = 50ms;
                break;
            case DurationCategory::hsm:
                defaults[durationCategory] = 30ms;
                break;
            case DurationCategory::enrolment:
                defaults[durationCategory] = 0ms;
                break;
        }
    }
    return defaults;
}

RuntimeConfigurationBase::RuntimeConfigurationBase()
    : mMetricsLogThresholdMs(defaultMetricsLogThresholdsMs())
{
}

void RuntimeConfigurationBase::setMetricsLogThresholdMs(DurationCategory category,
                                                        std::chrono::milliseconds thresholdMs)
{
    mMetricsLogThresholdMs[category] = thresholdMs;
    TLOG(INFO) << "metrics log threshold for " << magic_enum::enum_name(category) << " set to " << thresholdMs;
}

std::chrono::milliseconds RuntimeConfigurationBase::getMetricsLogThresholdMs(DurationCategory category) const
{
    return mMetricsLogThresholdMs.at(category);
}

std::map<DurationCategory, std::chrono::milliseconds> RuntimeConfigurationBase::getMetricsLogThresholdsMs() const

{
    return mMetricsLogThresholdMs;
}


}