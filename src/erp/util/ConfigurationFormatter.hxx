/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_CONFIGURATIONFORMATTER_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_CONFIGURATIONFORMATTER_HXX

#include <rapidjson/document.h>
#include <string>
#include <unordered_set>


class Configuration;
class RuntimeConfigurationGetter;
struct KeyData;

class ConfigurationFormatter
{
public:
    static std::string formatAsJson(const Configuration& config, const RuntimeConfigurationGetter& runtimeConfig,
                                    int flags);

private:
    static std::string getCategoryPath(int flags);
    static void appendRuntimeConfiguration(rapidjson::Document& document,
                                           const RuntimeConfigurationGetter& runtimeConfig);
    static void processConfOption(rapidjson::Document& document, std::unordered_set<std::string>& erpEnvVariables,
                                  const KeyData& confOption, int flags, const std::string& value,
                                  const std::string& defaultValue, bool modified);
};

#endif