// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "exporter/util/ConfigurationFormatter.hxx"
#include "exporter/util/RuntimeConfiguration.hxx"

#include <rapidjson/pointer.h>

namespace exporter
{

ConfigurationFormatter::ConfigurationFormatter(std::shared_ptr<const RuntimeConfiguration> runtimeConfig)
    : mRuntimeConfig(std::move(runtimeConfig))
{
}

void ConfigurationFormatter::appendRuntimeConfiguration(rapidjson::Document& document)
{
    const rapidjson::Pointer pausePointer("/runtime/" + std::string{RuntimeConfiguration::parameter_pause} + "/value");
    const rapidjson::Pointer throttlePointer("/runtime/" + std::string{RuntimeConfiguration::parameter_throttle} +
                                             "/value");
    RuntimeConfigurationGetter rcGetter(mRuntimeConfig);
    pausePointer.Set(document, rcGetter.isPaused());
    throttlePointer.Set(document, rcGetter.throttleValue().count());
}

}
