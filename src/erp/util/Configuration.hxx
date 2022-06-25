/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_CONFIGURATION_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_CONFIGURATION_HXX

#include "erp/util/Expect.hxx"
#include "erp/util/SafeString.hxx"

#include <erp/ErpConstants.hxx>
#include <rapidjson/document.h>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Environment.hxx"

class XmlValidator;


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
    IDP_NO_VALID_CERTIFICATE_UPDATE_INTERVAL_SECONDS,
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
    SERVICE_TASK_ACTIVATE_AUTHORED_ON_MUST_EQUAL_SIGNING_DATE,
    SERVICE_COMMUNICATION_MAX_MESSAGES,
    SERVICE_SUBSCRIPTION_SIGNING_KEY,
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
    TSL_TI_OCSP_PROXY_URL,
    TSL_FRAMEWORK_SSL_ROOT_CA_PATH,
    TSL_INITIAL_DOWNLOAD_URL,
    TSL_INITIAL_CA_DER_PATH,
    TSL_INITIAL_CA_DER_PATH_NEW,
    TSL_INITIAL_CA_DER_PATH_NEW_START,
    TSL_REFRESH_INTERVAL,
    TSL_DOWNLOAD_CIPHERS,
    XML_SCHEMA_MISC,
    FHIR_PROFILE_OLD_VALID_UNTIL,
    FHIR_PROFILE_OLD_XML_SCHEMA_KBV_VERSION,
    FHIR_PROFILE_OLD_XML_SCHEMA_KBV,
    FHIR_PROFILE_OLD_XML_SCHEMA_GEMATIK_VERSION,
    FHIR_PROFILE_OLD_XML_SCHEMA_GEMATIK,
    FHIR_PROFILE_VALID_FROM,
    FHIR_PROFILE_RENDER_FROM,
    FHIR_PROFILE_XML_SCHEMA_KBV_VERSION,
    FHIR_PROFILE_XML_SCHEMA_KBV,
    FHIR_PROFILE_XML_SCHEMA_GEMATIK_VERSION,
    FHIR_PROFILE_XML_SCHEMA_GEMATIK,
    HSM_CACHE_REFRESH_SECONDS,
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

    // Feature related configuration
    FEATURE_PKV,
    FEATURE_WORKFLOW_200,

    // Admin interface settings
    ADMIN_SERVER_INTERFACE,
    ADMIN_SERVER_PORT,
    ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS,
    ADMIN_CREDENTIALS,

    // Debug related configuration
    DEBUG_DISABLE_REGISTRATION,
    DEBUG_DISABLE_DOS_CHECK,
    DEBUG_ENABLE_HSM_MOCK,
    DEBUG_DISABLE_QES_ID_CHECK,
    DEBUG_ENABLE_MOCK_TSL_MANAGER
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
    explicit ConfigurationBase(const std::vector<KeyNames>& allKeyNames);

    std::optional<std::string> getStringValueInternal (KeyNames key) const;
    std::optional<SafeString> getSafeStringValueInternal (KeyNames key) const;
    std::optional<int> getIntValueInternal (KeyNames key) const;
    std::optional<bool> getBoolValueInternal (KeyNames key) const;
    std::vector<std::string> getArrayInternal (KeyNames key) const;
    std::vector<std::string> getOptionalArrayInternal (KeyNames key) const;
    std::map<std::string, std::vector<std::string>> getMapInternal (KeyNames key) const;

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

    int getOptionalIntValue (Key key, int defaultValue) const;
    std::optional<int> getOptionalIntValue (Key key) const       {return ConfigurationBase::getIntValueInternal(names_.strings(key));}
    int getIntValue (Key key) const                              {return ConfigurationBase::getIntValue(names_.strings(key));}

    std::string getStringValue (Key key) const                   {return ConfigurationBase::getStringValue(names_.strings(key));}
    std::string getOptionalStringValue (Key key, const std::string& defaultValue) const;
    std::optional<std::string> getOptionalStringValue (Key key) const                   {return ConfigurationBase::getOptionalStringValue(names_.strings(key));}
    SafeString getSafeStringValue (Key key) const                {return ConfigurationBase::getSafeStringValue(names_.strings(key));}
    SafeString getOptionalSafeStringValue (Key key, SafeString&& defaultValue) const;

    bool getBoolValue (Key key) const                            {return ConfigurationBase::getBoolValue(names_.strings(key));}
    bool getOptionalBoolValue (Key key, bool defaultValue) const;

    std::vector<std::string> getArray (Key key) const            {return ConfigurationBase::getArrayInternal(names_.strings(key));}
    std::vector<std::string> getOptionalArray(Key key) const     {return ConfigurationBase::getOptionalArrayInternal(names_.strings(key));}

    std::map<std::string, std::vector<std::string>> getMap(Key key) const {return ConfigurationBase::getMapInternal(names_.strings(key));}

    std::filesystem::path getPathValue(Key key) const            {return ConfigurationBase::getPathValue(names_.strings(key));}

    const char* getEnvironmentVariableName (Key key) const       {return names_.strings(key).environmentVariable.data();}

protected:
    ConfigurationTemplate()
        : ConfigurationBase(Names().allStrings())
    {
    }

    const Names names_;

    friend class ConfigurationTest;
};


template<class Key, class Names>
int ConfigurationTemplate<Key, Names>::getOptionalIntValue(Key key, int defaultValue) const
{
    if (names_.contains(key))
    {
        return ConfigurationBase::getOptionalIntValue(names_.strings(key), defaultValue);
    }
    return defaultValue;
}

template<class Key, class Names>
std::string ConfigurationTemplate<Key, Names>::getOptionalStringValue(Key key, const std::string& defaultValue) const
{
    if (names_.contains(key))
    {
        return ConfigurationBase::getOptionalStringValue(names_.strings(key), defaultValue);
    }
    return defaultValue;
}

template<class Key, class Names>
SafeString ConfigurationTemplate<Key, Names>::getOptionalSafeStringValue(Key key, SafeString&& defaultValue) const
{
    if (names_.contains(key))
    {
        return ConfigurationBase::getOptionalSafeStringValue(names_.strings(key), std::move(defaultValue));
    }
    return std::move(defaultValue);
}

template<class Key, class Names>
bool ConfigurationTemplate<Key, Names>::getOptionalBoolValue(Key key, bool defaultValue) const
{
    if (names_.contains(key))
    {
        return ConfigurationBase::getOptionalBoolValue(names_.strings(key), defaultValue);
    }
    return defaultValue;
}


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
template<class KeyType>
class ConfigurationKeyNamesBase
{
public:
    KeyNames strings (KeyType key) const
    {
        const auto entry = mNamesByKey.find(key);
        Expect(entry != mNamesByKey.end(), "unknown configuration key: " + std::string(magic_enum::enum_name(key)));
        return entry->second;
    }
    std::vector<KeyNames> allStrings() const
    {
        return getAllValues(mNamesByKey);
    }
    bool contains(KeyType key) const
    {
        return mNamesByKey.count(key) > 0;
    }

    virtual ~ConfigurationKeyNamesBase() = default;

protected:
    std::map<KeyType, KeyNames> mNamesByKey;
};

// contains all keys needed for operational, but no development or test related keys
class OpsConfigKeyNames : public ConfigurationKeyNamesBase<ConfigurationKey>
{
public:
    OpsConfigKeyNames();
};
// contains all keys needed for development, but no test related keys
class DevConfigKeyNames : public OpsConfigKeyNames
{
public:
    DevConfigKeyNames();
};

#ifdef NDEBUG
using ConfigurationKeyNames = OpsConfigKeyNames;
#else
using ConfigurationKeyNames = DevConfigKeyNames;
#endif

class Configuration : public ConfigurationTemplate<ConfigurationKey, ConfigurationKeyNames>
{
public:
    static const Configuration& instance();
    void check() const;

    [[nodiscard]] bool featureWf200Enabled() const;
    [[nodiscard]] bool featurePkvEnabled() const;
};


extern void configureXmlValidator(XmlValidator& xmlValidator);
/**
 * Return the optional port for the enrolment service.
 * If the enrolment service is not active in the current configuration then the returned optional remains empty.
 */
extern std::optional<uint16_t> getEnrolementServerPort(const uint16_t pcPort,
                                                       const uint16_t defaultEnrolmentServerPort);

#endif // ERP_PROCESSING_CONTEXT_UTIL_CONFIGURATION_HXX
