// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
// non-exclusively licensed to gematik GmbH

#ifndef ERP_PROCESSING_CONTEXT_UTIL_CONFIGURATIONFORMATTER_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_CONFIGURATIONFORMATTER_HXX

#include "shared/util/ConfigurationFormatter.hxx"

#include <gsl/gsl-lite.hpp>

namespace erp
{

class RuntimeConfiguration;

class ConfigurationFormatter : public ::ConfigurationFormatter
{
public:
    ~ConfigurationFormatter() override;
    explicit ConfigurationFormatter(gsl::not_null<std::shared_ptr<const RuntimeConfiguration>> runtimeConfig);
    void appendRuntimeConfiguration(rapidjson::Document& document) override;

private:
  std::shared_ptr<const RuntimeConfiguration> mRuntimeConfig;
};

}

#endif
