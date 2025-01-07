/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_CONFIGURATION_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_CONFIGURATION_HXX

#include "shared/ErpConstants.hxx"
#include "shared/model/ProfileType.hxx"
#include "shared/validation/SchemaType.hxx"
#include "fhirtools/validator/Severity.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/ExceptionWrapper.hxx"
#include "shared/util/SafeString.hxx"

#include <rapidjson/document.h>
#include <filesystem>
#include <list>
#include <magic_enum/magic_enum.hpp>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

class XmlValidator;

namespace fhirtools {
class ValidatorOptions;
class FhirResourceGroup;
class FhirResourceViewConfiguration;
class FhirResourceGroupConfiguration;
class FhirVersion;
}


enum class ConfigurationKey
{
    C_FD_SIG_ERP_VALIDATION_INTERVAL,
    ENROLMENT_SERVER_PORT,
    ENROLMENT_ACTIVATE_FOR_PORT,
    ENROLMENT_API_CREDENTIALS,
    ENTROPY_FETCH_INTERVAL_SECONDS,
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
    OCSP_C_FD_SIG_ERP_GRACE_PERIOD,
    OCSP_SMC_B_OSIG_GRACE_PERIOD,
    OCSP_NON_QES_GRACE_PERIOD,
    OCSP_QES_GRACE_PERIOD,
    SERVER_THREAD_COUNT,
    SERVER_CERTIFICATE,
    SERVER_PRIVATE_KEY,
    SERVER_PROXY_CERTIFICATE,
    SERVICE_TASK_ACTIVATE_ENTLASSREZEPT_VALIDITY_WD,
    SERVICE_TASK_ACTIVATE_HOLIDAYS,
    SERVICE_TASK_ACTIVATE_EASTER_CSV,
    SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION,
    SERVICE_TASK_ACTIVATE_KBV_VALIDATION_NON_LITERAL_AUTHOR_REF,
    SERVICE_TASK_ACTIVATE_ANR_VALIDATION_MODE,
    SERVICE_TASK_ACTIVATE_MVOID_VALIDATION_MODE,
    SERVICE_TASK_CLOSE_DEVICE_REF_TYPE,
    SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_REF_TYPE,
    SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_VERSION_ID,
    SERVICE_COMMUNICATION_MAX_MESSAGES,
    SERVICE_SUBSCRIPTION_SIGNING_KEY,
    PCR_SET,
    POSTGRES_HOST,
    POSTGRES_PORT,
    POSTGRES_USER,
    POSTGRES_PASSWORD,
    POSTGRES_DATABASE,
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
    POSTGRES_TARGET_SESSION_ATTRS,
    POSTGRES_CONNECTION_MAX_AGE_MINUTES,
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
    FHIR_STRUCTURE_DEFINITIONS,
    FHIR_VALIDATION_LEVELS_UNREFERENCED_BUNDLED_RESOURCE,
    FHIR_VALIDATION_LEVELS_UNREFERENCED_CONTAINED_RESOURCE,
    FHIR_VALIDATION_LEVELS_MANDATORY_RESOLVABLE_REFERENCE_FAILURE,
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
    TIMING_CATEGORIES,
    ZSTD_DICTIONARY_DIR,
    HTTPCLIENT_CONNECT_TIMEOUT_SECONDS,
    HTTPCLIENT_RESOLVE_TIMEOUT_MILLISECONDS,

    REDIS_DATABASE,
    REDIS_USER,
    REDIS_PASSWORD,
    REDIS_HOST,
    REDIS_PORT,
    REDIS_CERTIFICATE_PATH,
    REDIS_CONNECTION_TIMEOUT,
    REDIS_DOS_SOCKET_TIMEOUT,
    REDIS_CONNECTIONPOOL_SIZE,
    REDIS_SENTINEL_HOSTS,
    REDIS_SENTINEL_MASTER_NAME,
    REDIS_SENTINEL_SOCKET_TIMEOUT,

    TOKEN_ULIMIT_CALLS,
    TOKEN_ULIMIT_TIMESPAN_MS,

    REPORT_LEIPS_KEY_ENABLE,
    REPORT_LEIPS_KEY_REFRESH_INTERVAL_SECONDS,
    REPORT_LEIPS_KEY_CHECK_INTERVAL_SECONDS,
    REPORT_LEIPS_FAILED_KEY_CHECK_INTERVAL_SECONDS,

    // Admin interface settings
    ADMIN_SERVER_INTERFACE,
    ADMIN_SERVER_PORT,
    ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS,
    ADMIN_CREDENTIALS,
    ADMIN_RC_CREDENTIALS,

    // Debug related configuration
    DEBUG_DISABLE_REGISTRATION,
    DEBUG_DISABLE_DOS_CHECK,
    DEBUG_ENABLE_HSM_MOCK,
    DEBUG_DISABLE_QES_ID_CHECK,
    DEBUG_ENABLE_MOCK_TSL_MANAGER,
    VSDM_PROOF_VALIDITY_SECONDS,

    // Medication Exporter
    MEDICATION_EXPORTER_IS_PRODUCTION,

    MEDICATION_EXPORTER_SERVER_THREAD_COUNT,
    MEDICATION_EXPORTER_SERVER_IO_THREAD_COUNT,

    MEDICATION_EXPORTER_POSTGRES_HOST,
    MEDICATION_EXPORTER_POSTGRES_PORT,
    MEDICATION_EXPORTER_POSTGRES_USER,
    MEDICATION_EXPORTER_POSTGRES_PASSWORD,
    MEDICATION_EXPORTER_POSTGRES_DATABASE,
    MEDICATION_EXPORTER_POSTGRES_SSL_ROOT_CERTIFICATE_PATH,
    MEDICATION_EXPORTER_POSTGRES_SSL_CERTIFICATE_PATH,
    MEDICATION_EXPORTER_POSTGRES_SSL_KEY_PATH,
    MEDICATION_EXPORTER_POSTGRES_USESSL,
    MEDICATION_EXPORTER_POSTGRES_CONNECT_TIMEOUT_SECONDS,
    MEDICATION_EXPORTER_POSTGRES_ENABLE_SCRAM_AUTHENTICATION,
    MEDICATION_EXPORTER_POSTGRES_TCP_USER_TIMEOUT_MS,
    MEDICATION_EXPORTER_POSTGRES_KEEPALIVES_IDLE_SEC,
    MEDICATION_EXPORTER_POSTGRES_KEEPALIVES_INTERVAL_SEC,
    MEDICATION_EXPORTER_POSTGRES_KEEPALIVES_COUNT,
    MEDICATION_EXPORTER_POSTGRES_TARGET_SESSION_ATTRS,
    MEDICATION_EXPORTER_POSTGRES_CONNECTION_MAX_AGE_MINUTES,

    MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_CONNECT_TIMEOUT_SECONDS,
    MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_RESOLVE_TIMEOUT_MILLISECONDS,
    MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_ENFORCE_SERVER_AUTHENTICATION,
    MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_ENDPOINT,
    MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_USER_AGENT,
    MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_ERP_SUBMISSION_FUNCTION_ID,
    MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_EPA_AS_FQDN,
    MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_POOL_SIZE_PER_FQDN,
    MEDICATION_EXPORTER_EPA_PROCESSING_OUTCOME_RETRY_DELAY_SECONDS,
    MEDICATION_EXPORTER_EPA_CONFLICT_WAIT_MINUTES,

    MEDICATION_EXPORTER_FHIR_STRUCTURE_DEFINITIONS,

    MEDICATION_EXPORTER_ADMIN_SERVER_INTERFACE,
    MEDICATION_EXPORTER_ADMIN_SERVER_PORT,
    MEDICATION_EXPORTER_ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS,
    MEDICATION_EXPORTER_ADMIN_CREDENTIALS,
    MEDICATION_EXPORTER_ADMIN_RC_CREDENTIALS,

    MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_CONNECT_TIMEOUT_MILLISECONDS,
    MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_SSL_HANDSHAKE_TIMEOUT_MILLISECONDS,
    MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_MESSAGE_SEND_TIMEOUT_MILLISECONDS,
    MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_MESSAGE_RECEIVE_MILLISECONDS,
    MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_RETRY_TIMEOUT_MILLISECONDS,
    MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_RETRIES_PER_ADDRESS,
    MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_RESOLVE_TIMEOUT_MILLISECONDS,
    MEDICATION_EXPORTER_OCSP_EPA_GRACE_PERIOD,
};


/**
 * Generic representation of a configuration key's associated environment variable name and json path.
 */
struct KeyData
{
    std::string_view environmentVariable;
    std::string_view jsonPath;
    enum ConfigurationKeyFlags
    {
        none        = 0,
        credential  = 1 << 0,
        array       = 1 << 1,
        deprecated  = 1 << 2,
        categoryEnvironment = 1 << 3,
        categoryFunctional = 1 << 4,
        categoryFunctionalStatic = 1 << 5,
        categoryDebug = 1 << 6,
        categoryRuntime = 1 << 7,
        all = INT_MAX,
    };
    int flags = none;
    std::string_view description;
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

    struct ProfileTypeRequirement {
        std::set<std::string> onlyViews{};
    };

    struct ERP {
        constexpr static std::string_view fhirResourceGroups = "/erp/fhir/resource-groups";
        constexpr static std::string_view fhirResourceViews = "/erp/fhir/resource-views";
        constexpr static std::string_view synthesizeCodesystemPath = "/erp/fhir/synthesize-codesystems";
        constexpr static std::string_view synthesizeValuesetPath = "/erp/fhir/synthesize-valuesets";
        constexpr static ConfigurationKey FHIR_STRUCTURE_DEFINITIONS = ConfigurationKey::FHIR_STRUCTURE_DEFINITIONS;
        static const std::map<model::ProfileType, ProfileTypeRequirement> requiredProfiles;
    };
    struct MedicationExporter {
        constexpr static std::string_view fhirResourceGroups = "/erp-medication-exporter/fhir/resource-groups";
        constexpr static std::string_view fhirResourceViews = "/erp-medication-exporter/fhir/resource-views";
        constexpr static std::string_view synthesizeCodesystemPath = "/erp-medication-exporter/fhir/synthesize-codesystems";
        constexpr static std::string_view synthesizeValuesetPath = "/erp-medication-exporter/fhir/synthesize-valuesets";
        constexpr static ConfigurationKey FHIR_STRUCTURE_DEFINITIONS = ConfigurationKey::MEDICATION_EXPORTER_FHIR_STRUCTURE_DEFINITIONS;
        static const std::map<model::ProfileType, ProfileTypeRequirement> requiredProfiles;
    };
    const std::string& serverHost() const;
    uint16_t serverPort() const;

protected:
    explicit ConfigurationBase(const std::vector<KeyData>& allKeyNames);

    std::optional<std::string> getStringValueInternal (KeyData key) const;
    std::optional<SafeString> getSafeStringValueInternal (KeyData key) const;
    std::optional<int> getIntValueInternal (KeyData key) const;
    std::optional<bool> getBoolValueInternal (KeyData key) const;
    std::vector<std::string> getArrayInternal (KeyData key) const;
    std::vector<std::string> getOptionalArrayInternal (KeyData key) const;
    std::map<std::string, std::vector<std::string>> getMapInternal (KeyData key) const;

    int getOptionalIntValue (KeyData key, int defaultValue) const;
    int getIntValue (KeyData key) const;

    std::string getStringValue (KeyData key) const;
    std::string getOptionalStringValue (KeyData key, const std::string& defaultValue) const;
    std::optional<std::string> getOptionalStringValue (KeyData key) const;
    SafeString getSafeStringValue (KeyData key) const;
    SafeString getOptionalSafeStringValue (KeyData key, SafeString defaultValue) const;
    std::optional<SafeString> getOptionalSafeStringValue (KeyData key) const;

    std::filesystem::path getPathValue(KeyData key) const;

    bool getBoolValue (KeyData key) const;
    bool getOptionalBoolValue (KeyData key, bool defaultValue) const;

    std::optional<std::string> getOptionalStringFromJson(KeyData key) const;
    std::vector<std::string> getOptionalArrayFromJson(KeyData key) const;

    const rapidjson::Value* getJsonValue(KeyData key) const;

    [[nodiscard]] std::list<std::pair<std::string, fhirtools::FhirVersion>>
    resourceList(const std::string& jsonPath) const;


private:
    bool lookupKey(const std::string& pathPrefix, const std::string& jsonKey);

    rapidjson::Document mDocument;
    std::map<std::string, const rapidjson::Value*> mValuesByKey;
    std::string mServerHost;
    uint16_t mServerPort;
};

namespace config
{
template<typename T>
concept ProcessType = std::same_as<T, ConfigurationBase::ERP> || std::same_as<T, ConfigurationBase::MedicationExporter>;
}

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
    std::optional<SafeString> getOptionalSafeStringValue (Key key) const                { return ConfigurationBase::getOptionalSafeStringValue(names_.strings(key)); }
    SafeString getOptionalSafeStringValue (Key key, SafeString&& defaultValue) const;

    bool getBoolValue (Key key) const                            {return ConfigurationBase::getBoolValue(names_.strings(key));}
    bool getOptionalBoolValue (Key key, bool defaultValue) const;

    std::vector<std::string> getArray (Key key) const            {return ConfigurationBase::getArrayInternal(names_.strings(key));}
    std::vector<std::string> getOptionalArray(Key key) const     {return ConfigurationBase::getOptionalArrayInternal(names_.strings(key));}

    std::map<std::string, std::vector<std::string>> getMap(Key key) const {return ConfigurationBase::getMapInternal(names_.strings(key));}

    std::filesystem::path getPathValue(Key key) const            {return ConfigurationBase::getPathValue(names_.strings(key));}

    const char* getEnvironmentVariableName (Key key) const       {return names_.strings(key).environmentVariable.data();}

    std::optional<std::string> getOptionalStringFromJson(Key key) const
    {
        return ConfigurationBase::getOptionalStringFromJson(names_.strings(key));
    }

    std::vector<std::string> getOptionalArrayFromJson(Key key) const
    {
        return ConfigurationBase::getOptionalArrayFromJson(names_.strings(key));
    }

    template<typename EnumT>
        requires std::is_enum_v<EnumT>
    [[nodiscard]] EnumT getOptional(Key key, std::type_identity_t<EnumT> defaultValue) const;

    template<typename EnumT>
    [[nodiscard]] EnumT get(Key key) const
    requires std::is_enum_v<EnumT>;

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

template<typename Key, typename Names>
template<typename EnumT>
requires std::is_enum_v<EnumT>
EnumT ConfigurationTemplate<Key, Names>::getOptional(Key key, std::type_identity_t<EnumT> defaultValue) const
{
    using namespace std::string_literals;
    auto enumStr = getOptionalStringValue(key, std::string{magic_enum::enum_name(defaultValue)});
    auto asEnum = magic_enum::enum_cast<EnumT>(enumStr);
    if (! asEnum.has_value())
    {
        throw ExceptionWrapper<std::logic_error>::create(
            {__FILE__, __LINE__}, "invalid value for "s.append(magic_enum::enum_name(key)) + ": " + enumStr);
    }
    return *asEnum;
}

template<typename Key, typename Names>
template<typename EnumT>
EnumT ConfigurationTemplate<Key, Names>::get(Key key) const
requires std::is_enum_v<EnumT>
{
    using namespace std::string_literals;
    auto enumStr = getStringValue(key);
    auto asEnum = magic_enum::enum_cast<EnumT>(enumStr);
    if (! asEnum.has_value())
    {
        throw ExceptionWrapper<std::logic_error>::create(
            {__FILE__, __LINE__}, "invalid value for "s.append(magic_enum::enum_name(key)) + ": " + enumStr);
    }
    return *asEnum;
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
 * A provider template of KeyData objects for all keys in TConfigurationKey.
 */
template<class KeyType>
class ConfigurationKeyNamesBase
{
public:
    KeyData strings (KeyType key) const
    {
        const auto entry = mNamesByKey.find(key);
        if (entry == mNamesByKey.end())
        {
            throw ExceptionWrapper<std::runtime_error>::create(
                {__FILE__, __LINE__}, "unknown configuration key: " + std::string(magic_enum::enum_name(key)));
        }
        return entry->second;
    }
    std::vector<KeyData> allStrings() const
    {
        return getAllValues(mNamesByKey);
    }
    bool contains(KeyType key) const
    {
        return mNamesByKey.count(key) > 0;
    }

    std::vector<KeyType> allKeys() const
    {
        std::vector<KeyType> keys;
        keys.reserve(mNamesByKey.size());
        for (const auto& key : mNamesByKey)
        {
            keys.push_back(key.first);
        }

        return keys;
    }

    virtual ~ConfigurationKeyNamesBase() = default;

protected:
    std::map<KeyType, KeyData> mNamesByKey;
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
    enum class OnUnknownExtension
    {
        ignore,
        report,
        reject
    };
    enum class NonLiteralAuthorRefMode
    {
        allow, deny,
    };
    enum class AnrChecksumValidationMode {
        warning, error,
    };
    enum class PrescriptionDigestRefType
    {
        relative,
        uuid
    };
    enum class DeviceRefType
    {
        url,
        uuid
    };
    enum class MvoIdValidationMode : uint8_t
    {
        disable,
        error
    };

    static const Configuration& instance();
    void check() const;

    [[nodiscard]] OnUnknownExtension kbvValidationOnUnknownExtension() const;
    [[nodiscard]] NonLiteralAuthorRefMode kbvValidationNonLiteralAuthorRef() const;
    [[nodiscard]] bool timingLoggingEnabled(const std::string& category) const;
    fhirtools::ValidatorOptions defaultValidatorOptions(model::ProfileType profileType) const;
    AnrChecksumValidationMode anrChecksumValidationMode() const;
    [[nodiscard]] PrescriptionDigestRefType prescriptionDigestRefType() const;
    [[nodiscard]] DeviceRefType closeTaskDeviceRefType() const;
    [[nodiscard]] MvoIdValidationMode mvoIdValidationMode() const;

    template <config::ProcessType ProcessT>
    [[nodiscard]] fhirtools::FhirResourceGroupConfiguration fhirResourceGroupConfiguration() const;
    template <config::ProcessType ProcessT>
    [[nodiscard]] fhirtools::FhirResourceViewConfiguration fhirResourceViewConfiguration() const;
    template <config::ProcessType ProcessT>
    [[nodiscard]] std::list<std::pair<std::string, fhirtools::FhirVersion>> synthesizeCodesystem() const;
    template <config::ProcessType ProcessT>
    [[nodiscard]] std::list<std::pair<std::string, fhirtools::FhirVersion>> synthesizeValuesets() const;

    struct EpaFQDN {
        std::string hostName;
        uint16_t port{};
        size_t teeConnectionCount = 8;
    };

    std::vector<EpaFQDN> epaFQDNs() const;

};


extern void configureXmlValidator(XmlValidator& xmlValidator);
/**
 * Return the optional port for the enrolment service.
 * If the enrolment service is not active in the current configuration then the returned optional remains empty.
 */
extern std::optional<uint16_t> getEnrolementServerPort(const uint16_t pcPort,
                                                       const uint16_t defaultEnrolmentServerPort);

extern template fhirtools::Severity
    ConfigurationTemplate<ConfigurationKey, ConfigurationKeyNames>::getOptional<fhirtools::Severity>(
        ConfigurationKey, std::type_identity_t<fhirtools::Severity>) const;


extern template fhirtools::FhirResourceGroupConfiguration Configuration::fhirResourceGroupConfiguration<ConfigurationBase::ERP>() const;
extern template fhirtools::FhirResourceViewConfiguration Configuration::fhirResourceViewConfiguration<ConfigurationBase::ERP>() const;
extern template std::list<std::pair<std::string, fhirtools::FhirVersion>> Configuration::synthesizeCodesystem<ConfigurationBase::ERP>() const;
extern template std::list<std::pair<std::string, fhirtools::FhirVersion>> Configuration::synthesizeValuesets<ConfigurationBase::ERP>() const;

extern template fhirtools::FhirResourceGroupConfiguration Configuration::fhirResourceGroupConfiguration<ConfigurationBase::MedicationExporter>() const;
extern template fhirtools::FhirResourceViewConfiguration Configuration::fhirResourceViewConfiguration<ConfigurationBase::MedicationExporter>() const;
extern template std::list<std::pair<std::string, fhirtools::FhirVersion>> Configuration::synthesizeCodesystem<ConfigurationBase::MedicationExporter>() const;
extern template std::list<std::pair<std::string, fhirtools::FhirVersion>> Configuration::synthesizeValuesets<ConfigurationBase::MedicationExporter>() const;


#endif // ERP_PROCESSING_CONTEXT_UTIL_CONFIGURATION_HXX
