/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_SHARED_UTIL_CONFIGURATIONFORMATTER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_SHARED_UTIL_CONFIGURATIONFORMATTER_HXX

#include <rapidjson/document.h>
#include <string>
#include <unordered_set>


class Configuration;
struct KeyData;

class ConfigurationFormatter
{
public:
    virtual ~ConfigurationFormatter() = default;
    std::string formatAsJson(const Configuration& config, int flags);

private:
    virtual void appendRuntimeConfiguration(rapidjson::Document& document) = 0;
    std::string getCategoryPath(int flags);
    void processConfOption(rapidjson::Document& document, std::unordered_set<std::string>& erpEnvVariables,
                           const KeyData& confOption, int flags, const std::string& value,
                           const std::string& defaultValue, bool modified);
};

#endif