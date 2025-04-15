// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef EXPORTER_UTIL_CONFIGURATIONFORMATTER_HXX
#define EXPORTER_UTIL_CONFIGURATIONFORMATTER_HXX

#include "shared/util/ConfigurationFormatter.hxx"

namespace exporter
{
class RuntimeConfiguration;

class ConfigurationFormatter : public ::ConfigurationFormatter
{
public:
    ConfigurationFormatter(std::shared_ptr<const RuntimeConfiguration> runtimeConfig);
    void appendRuntimeConfiguration(rapidjson::Document& document) override;

private:
    std::shared_ptr<const RuntimeConfiguration> mRuntimeConfig;
};

}

#endif
