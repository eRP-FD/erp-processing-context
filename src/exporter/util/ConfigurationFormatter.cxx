// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "exporter/util/ConfigurationFormatter.hxx"
#include "exporter/util/RuntimeConfiguration.hxx"

#include <magic_enum/magic_enum.hpp>
#include <rapidjson/pointer.h>

#include "fhirtools/repository/views/FhirResourceViewConfiguration.hxx"
#include "shared/util/Configuration.hxx"

namespace exporter
{

ConfigurationFormatter::ConfigurationFormatter(std::shared_ptr<const RuntimeConfiguration> runtimeConfig)
    : mRuntimeConfig(std::move(runtimeConfig))
{
}

void ConfigurationFormatter::appendRuntimeConfiguration(rapidjson::Document& document)
{
    const rapidjson::Pointer pausePointer("/runtime/" + std::string{RuntimeConfiguration::parameter_pause} + "/value");
    const rapidjson::Pointer pauseEpaPointer("/runtime/" + std::string{RuntimeConfiguration::parameter_pause} + "/" +
                                             std::string{RuntimeConfiguration::parameter_epa} + "/value");
    const rapidjson::Pointer pauseTRezeptPointer("/runtime/" + std::string{RuntimeConfiguration::parameter_pause} +
                                                 "/" + std::string{RuntimeConfiguration::parameter_t_rezept} +
                                                 "/value");
    const rapidjson::Pointer throttlePointer("/runtime/" + std::string{RuntimeConfiguration::parameter_throttle} +
                                             "/value");
    const RuntimeConfigurationGetter rcGetter(mRuntimeConfig);
    pausePointer.Set(
        document, std::ranges::all_of(magic_enum::enum_values<RuntimeConfiguration::ProcessorType>(), [&](auto pType) {
            return rcGetter.isPaused(pType);
        }));
    for (const auto processor : magic_enum::enum_values<RuntimeConfiguration::ProcessorType>())
    {
        switch (processor)
        {
            case RuntimeConfiguration::ProcessorType::EPA:
                pauseEpaPointer.Set(document, rcGetter.isPaused(processor));
                break;
            case RuntimeConfiguration::ProcessorType::T_REZEPT:
                pauseTRezeptPointer.Set(document, rcGetter.isPaused(processor));
                break;
        }
    }
    throttlePointer.Set(document, rcGetter.throttleValue().count());
    for (const auto& [parameterMetricsLogThresholdMs, category] :
         RuntimeConfiguration::parameter_metrics_category_log_threshold_ms)
    {
        const rapidjson::Pointer metricsLogThresholdMsPointer("/runtime/" + parameterMetricsLogThresholdMs + "/value");
        metricsLogThresholdMsPointer.Set(document,
                                         rapidjson::Value(rcGetter.getMetricsLogThresholdMs(category).count()));
    }
}

void ConfigurationFormatter::appendFhirPackagesConfiguration(rapidjson::Document& document)
{
    const fhirtools::FhirResourceViewConfiguration resourceViewConfigs =
        Configuration::instance().fhirResourceViewConfiguration<ConfigurationBase::MedicationExporter>();
    ::ConfigurationFormatter::appendFhirPackagesConfiguration(document, resourceViewConfigs);
}
}
