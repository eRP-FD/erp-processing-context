#ifndef ERP_PROCESSING_CONTEXT_UTIL_CONFIGURATION_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_CONFIGURATION_HXX

#include "erp/util/SafeString.hxx"
#include "erp/util/Expect.hxx"

#include <filesystem>
#include <string>
#include <optional>
#include <rapidjson/document.h>
#include <vector>
#include <map>


enum class ConfigurationKey
{
    C_FD_SIG_ERP,
    ECIES_CERTIFICATE,
    ENROLMENT_SERVER_PORT,
    ENROLMENT_ACTIVATE_FOR_PORT,
    ENROLMENT_API_CREDENTIALS,
    ENTROPY_FETCH_INTERVAL_SECONDS,
    FHIR_STRUCTURE_DEFINITIONS,
    IDP_REGISTERED_FD_URI,
    IDP_WELLKNOWN_HOST,
    IDP_WELLKNOWN_PATH,
    IDP_CERTIFICATE_MAX_AGE_HOURS,
    IDP_UPDATE_ENDPOINT,
    IDP_UPDATE_ENDPOINT_SSL_ROOT_CA_PATH,
    IDP_UPDATE_INTERVAL_MINUTES,
    JSON_META_SCHEMA,
    JSON_SCHEMA,
    OCSP_NON_QES_GRACE_PERIOD,
    OCSP_QES_GRACE_PERIOD,
    SERVER_THREAD_COUNT,
    SERVER_CERTIFICATE,
    SERVER_PRIVATE_KEY,
    SERVER_REQUEST_PATH,
    SERVER_PROXY_CERTIFICATE,
    SERVICE_TASK_ACTIVATE_ENTLASSREZEPT_VALIDITY_WD,
    SERVICE_TASK_ACTIVATE_HOLIDAYS,
    SERVICE_TASK_ACTIVATE_EASTER_CSV,
    SERVICE_TASK_ACTIVATE_KBV_VALIDATION,
    SERVICE_COMMUNICATION_MAX_MESSAGES,
    PCR_SET,
    POSTGRES_HOST,
    POSTGRES_PORT,
    POSTGRES_USER,
    POSTGRES_PASSWORD,
    POSTGRES_DATABASE,
    POSTGRES_CERTIFICATE,
    POSTGRES_SSL_ROOT_CERTIFICATE_PATH,
    POSTGRES_SSL_CERTIFICATE_PATH,
    POSTGRES_SSL_KEY_PATH,
    POSTGRES_USESSL,
    POSTGRES_CONNECT_TIMEOUT_SECONDS,
    POSTGRES_ENABLE_SCRAM_AUTHENTICATION,
    POSTGRES_TCP_USER_TIMEOUT_MS,
    POSTGRES_KEEPALIVES_IDLE_SEC,
    POSTGRES_KEEPALIVES_INTERVAL_SEC,
    POSTGRES_KEEPALIVES_COUNT,
    PUBLIC_E_PRESCRIPTION_SERVICE_URL,
    REGISTRATION_HEARTBEAT_INTERVAL_SEC,
    TSL_FRAMEWORK_SSL_ROOT_CA_PATH,
    TSL_INITIAL_DOWNLOAD_URL,
    TSL_INITIAL_CA_DER_PATH,
    TSL_INITIAL_CA_DER_PATH_NEW,
    TSL_INITIAL_CA_DER_PATH_NEW_START,
    TSL_REFRESH_INTERVAL,
    TSL_DOWNLOAD_CIPHERS,
    XML_SCHEMA,
    HSM_DEVICE,
    HSM_MAX_SESSION_COUNT,
    HSM_WORK_USERNAME,
    HSM_WORK_PASSWORD,
    HSM_WORK_KEYSPEC,
    HSM_SETUP_USERNAME,
    HSM_SETUP_PASSWORD,
    HSM_SETUP_KEYSPEC,
    HSM_CONNECT_TIMEOUT_SECONDS,
    HSM_READ_TIMEOUT_SECONDS,
    HSM_IDLE_TIMEOUT_SECONDS,
    HSM_RECONNECT_INTERVAL_SECONDS,
    DEPRECATED_HSM_USERNAME,
    DEPRECATED_HSM_PASSWORD,
    TEE_TOKEN_UPDATE_SECONDS,
    TEE_TOKEN_RETRY_SECONDS,
    TEE_TOKEN_BLOB,
    ZSTD_DICTIONARY_DIR,
    HTTPCLIENT_CONNECT_TIMEOUT_SECONDS,

    REDIS_DATABASE,
    REDIS_USER,
    REDIS_PASSWORD,
    REDIS_HOST,
    REDIS_PORT,
    REDIS_CERTIFICATE_PATH,
    REDIS_CONNECTION_TIMEOUT,
    REDIS_CONNECTIONPOOL_SIZE,

    TOKEN_ULIMIT_CALLS,
    TOKEN_ULIMIT_TIMESPAN_MS,

    // Debug related configuration
    DEBUG_DISABLE_REGISTRATION,
    DEBUG_DISABLE_DOS_CHECK,
    DEBUG_ENABLE_HSM_MOCK,
    DEBUG_DISABLE_PROXY_AUTHENTICATION,
    DEBUG_DISABLE_ENROLMENT_API_AUTH
};


/**
 * Generic representation of a configuration key's associated environment variable name and json path.
 */
struct KeyNames
{
    const std::string_view environmentVariable;
    const std::string_view jsonPath;
};

/**
 * The base class of the Configuration is key agnostic and can be used with every concret configuration class
 * that provides KeyNames objects for its keys.
 */
class ConfigurationBase
{
public:
    constexpr static std::string_view ServerHostEnvVar = "ERP_SERVER_HOST";
    constexpr static std::string_view ServerPortEnvVar = "ERP_SERVER_PORT";

    const std::string& serverHost() const;
    uint16_t serverPort() const;

protected:
    ConfigurationBase (const std::vector<KeyNames>& allKeyNames);

    std::optional<std::string> getStringValueInternal (KeyNames key) const;
    std::optional<SafeString> getSafeStringValueInternal (KeyNames key) const;
    std::optional<int> getIntValueInternal (KeyNames key) const;
    std::optional<bool> getBoolValueInternal (KeyNames key) const;
    std::vector<std::string> getArrayInternal (KeyNames key) const;

    int getOptionalIntValue (KeyNames key, int defaultValue) const;
    int getIntValue (KeyNames key) const;

    std::string getStringValue (KeyNames key) const;
    std::string getOptionalStringValue (KeyNames key, const std::string& defaultValue) const;
    std::optional<std::string> getOptionalStringValue (KeyNames key) const;
    SafeString getSafeStringValue (KeyNames key) const;
    SafeString getOptionalSafeStringValue (KeyNames key, SafeString defaultValue) const;

    std::filesystem::path getPathValue(KeyNames key) const;

    bool getBoolValue (KeyNames key) const;
    bool getOptionalBoolValue (KeyNames key, bool defaultValue) const;

private:
    bool lookupKey(const std::string& pathPrefix, const std::string& jsonKey);

    rapidjson::Document mDocument;
    std::map<std::string, const rapidjson::Value*> mValuesByKey;
    std::string mServerHost;
    uint16_t mServerPort;
};


/**
 * This template class allows different concrete configuration classes to use different key enums.
 * This is necessary because C++ does not allow extension of existing enums
 */
template<class Key, class Names>
class ConfigurationTemplate : public ConfigurationBase
{
public:
    ConfigurationTemplate(const ConfigurationTemplate&) = delete;
    ConfigurationTemplate(ConfigurationTemplate&&) = delete;
    ConfigurationTemplate& operator=(const ConfigurationTemplate&) = delete;
    ConfigurationTemplate& operator=(ConfigurationTemplate&&) = delete;

    static const ConfigurationTemplate& instance()
    {
        static ConfigurationTemplate instance;
        return instance;
    }

    int getOptionalIntValue (Key key, int defaultValue) const    {return ConfigurationBase::getOptionalIntValue(Names::strings(key), defaultValue);}
    std::optional<int> getOptionalIntValue (Key key) const       {return ConfigurationBase::getIntValueInternal(Names::strings(key));}
    int getIntValue (Key key) const                              {return ConfigurationBase::getIntValue(Names::strings(key));}

    std::string getStringValue (Key key) const                   {return ConfigurationBase::getStringValue(Names::strings(key));}
    std::string getOptionalStringValue (Key key, const std::string& defaultValue) const {return ConfigurationBase::getOptionalStringValue(Names::strings(key), defaultValue);}
    std::optional<std::string> getOptionalStringValue (Key key) const                   {return ConfigurationBase::getOptionalStringValue(Names::strings(key));}
    SafeString getSafeStringValue (Key key) const                {return ConfigurationBase::getSafeStringValue(Names::strings(key));}
    SafeString getOptionalSafeStringValue (Key key, SafeString&& defaultValue) const    {return ConfigurationBase::getOptionalSafeStringValue(Names::strings(key), std::move(defaultValue));}

    bool getBoolValue (Key key) const                            {return ConfigurationBase::getBoolValue(Names::strings(key));}
    bool getOptionalBoolValue (Key key, bool defaultValue) const {return ConfigurationBase::getOptionalBoolValue(Names::strings(key), defaultValue);}

    std::vector<std::string> getArray (Key key) const            {return ConfigurationBase::getArrayInternal(Names::strings(key));}

    std::filesystem::path getPathValue(Key key) const            {return ConfigurationBase::getPathValue(Names::strings(key));}

    static const char* getEnvironmentVariableName (Key key) {return Names::strings(key).environmentVariable.data();}

private:
    ConfigurationTemplate()
    : ConfigurationBase(Names::allStrings())
    { }

    friend class ConfigurationTest;
};

template<typename TKeyValueContainer>
std::vector<typename TKeyValueContainer::mapped_type> getAllValues(const TKeyValueContainer& keyValueContainer)
{
    std::vector<typename TKeyValueContainer::mapped_type> allValues;
    allValues.reserve(keyValueContainer.size());
    for(const auto& [key, value] : keyValueContainer)
    {
        allValues.emplace_back(value);
    }
    return allValues;
}


/**
 * A provider template of KeyNames objects for all keys in TConfigurationKey.
 */
template<typename TConfigurationKey>
class ConfigurationKeyNamesTemplate
{
public:
    static KeyNames strings (TConfigurationKey key)
    {
        const auto entry = mNamesByKey.find(key);
        Expect(entry != mNamesByKey.end(), "unknown configuration key");
        return entry->second;
    }
    static std::vector<KeyNames> allStrings()
    {
        return getAllValues(mNamesByKey);
    }

private:
    static std::map<TConfigurationKey, KeyNames> mNamesByKey;
};

using ConfigurationKeyNames = ConfigurationKeyNamesTemplate<ConfigurationKey>;

using Configuration = ConfigurationTemplate<ConfigurationKey, ConfigurationKeyNames>;

template<>
std::map<ConfigurationKey,KeyNames> ConfigurationKeyNamesTemplate<ConfigurationKey>::mNamesByKey;

#endif
