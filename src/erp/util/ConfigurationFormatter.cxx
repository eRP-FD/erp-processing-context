// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/util/ConfigurationFormatter.hxx"
#include "erp/util/RuntimeConfiguration.hxx"

#include <rapidjson/pointer.h>

#include "fhirtools/repository/views/FhirResourceViewConfiguration.hxx"
#include "shared/util/Configuration.hxx"

namespace erp
{

ConfigurationFormatter::~ConfigurationFormatter() = default;

ConfigurationFormatter::ConfigurationFormatter(gsl::not_null<std::shared_ptr<const RuntimeConfiguration>> runtimeConfig)
    : ::ConfigurationFormatter()
    , mRuntimeConfig(std::move(runtimeConfig))
{
}

void ConfigurationFormatter::appendRuntimeConfiguration(rapidjson::Document& document)
{
    const rapidjson::Pointer acceptPN3Pointer("/runtime/" + std::string{RuntimeConfiguration::parameter_accept_pn3} +
                                              "/value");
    RuntimeConfigurationGetter rcGetter(mRuntimeConfig);

    acceptPN3Pointer.Set(
        document, rapidjson::Value((rcGetter.isAcceptPN3Enabled() ? "true" : "false"), document.GetAllocator()),
        document.GetAllocator());
    if (rcGetter.isAcceptPN3Enabled())
    {
        const rapidjson::Pointer acceptPN3ExpiryPointer(
            "/runtime/" + std::string{RuntimeConfiguration::parameter_accept_pn3_expiry} + "/value");
        acceptPN3ExpiryPointer.Set(
            document, rapidjson::Value(rcGetter.getAcceptPN3Expiry().toXsDateTime(), document.GetAllocator()),
            document.GetAllocator());
    }
    const rapidjson::Pointer acceptPN3MaxActive(
        "/runtime/" + std::string{RuntimeConfiguration::parameter_accept_pn3_max_active} + "/value");

    // std::chrono::hh_mm_ss::operator<< not yet implemented in gcc-12.
    const std::chrono::hh_mm_ss hhmmss(RuntimeConfiguration::accept_pn3_max_active);
    acceptPN3MaxActive.Set(
        document,
        rapidjson::Value(
            (std::ostringstream{} << hhmmss.hours() << " " << hhmmss.minutes() << " " << hhmmss.seconds()).str(),
            document.GetAllocator()),
        document.GetAllocator());

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
        Configuration::instance().fhirResourceViewConfiguration<ConfigurationBase::ERP>();
    ::ConfigurationFormatter::appendFhirPackagesConfiguration(document, resourceViewConfigs);
}
}
