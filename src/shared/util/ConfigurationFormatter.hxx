/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_SHARED_UTIL_CONFIGURATIONFORMATTER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_SHARED_UTIL_CONFIGURATIONFORMATTER_HXX

#include <memory>
#include <string>
#include <unordered_set>
#include <rapidjson/document.h>


class Configuration;
enum class ConfigurationKey;
enum class ConfigurationKeyFlags : uint16_t;
struct KeyData;
class OpsConfigKeyNames;

namespace fhirtools {
class FhirResourceViewConfiguration;
};
class ConfigurationFormatter
{
public:
    ConfigurationFormatter();
    virtual ~ConfigurationFormatter();
    std::string formatAsJson(const Configuration& config, ConfigurationKeyFlags flags);

    void addConfigOption(rapidjson::Document& document, const Configuration& config, ConfigurationKey confKey);
    static void addConfigOption(rapidjson::Document& document, const Configuration& config, ConfigurationKey confKey,
                                const KeyData& confOption);

protected:
    static void appendFhirPackagesConfiguration(rapidjson::Document& document,
                                                const fhirtools::FhirResourceViewConfiguration& config);

private:

    virtual void appendRuntimeConfiguration(rapidjson::Document& document) = 0;
    virtual void appendFhirPackagesConfiguration(rapidjson::Document& document) = 0;
    static std::string getCategoryPath(ConfigurationKeyFlags flags);
    static std::string baseJsonPath(const KeyData& confOption);
    static void addCommonMembers(rapidjson::Document& document, const KeyData& confOption, const std::string& key,
                                 bool modified);
    static void addValueMembers(rapidjson::Document& document, const std::string& key, const std::string& value,
                                const std::string& defaultValue, const KeyData& confOption);
    static void addFileMembers(rapidjson::Document& document, const std::string& key, const std::string& value,
                               const std::string& defaultValue, const KeyData& confOption);
    static void addPemMembers(rapidjson::Document& document, const std::string& key, const std::string& value,
                              const std::string& defaultValue, const KeyData& confOption);

    std::unique_ptr<OpsConfigKeyNames> mConfNames;
};

#endif
