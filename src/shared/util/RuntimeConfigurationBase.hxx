// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include "shared/model/Timestamp.hxx"
#include "shared/util/DurationCategory.hxx"

#include <map>

namespace shared
{
class RuntimeConfigurationBase
{
public:
    static const std::map<std::string, DurationCategory> parameter_metrics_category_log_threshold_ms;

    static std::map<DurationCategory, std::chrono::milliseconds>
    defaultMetricsLogThresholdsMs();

protected:
    RuntimeConfigurationBase();
    void setMetricsLogThresholdMs(DurationCategory category, std::chrono::milliseconds thresholdMs);
    std::chrono::milliseconds getMetricsLogThresholdMs(DurationCategory category) const;
    std::map<DurationCategory, std::chrono::milliseconds> getMetricsLogThresholdsMs() const;

private:
    std::map<DurationCategory, std::chrono::milliseconds> mMetricsLogThresholdMs;
};

}
