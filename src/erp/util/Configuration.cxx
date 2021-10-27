/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/Configuration.hxx"

#include "erp/ErpConstants.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/GLog.hxx"
#include "erp/util/InvalidConfigurationException.hxx"
#include "erp/util/String.hxx"

#include <stdexcept>
#include <string>


// Compile the rapidjson assert so that it calls Expects() and will throw an exception instead of just aborting (crashing)
// the application.
#undef RAPIDJSON_ASSERT
#define RAPIDJSON_ASSERT(x) Expect(x, "rapidjson error")
#include <rapidjson/pointer.h>
#include <rapidjson/error/en.h>


namespace {
    constexpr uint16_t DefaultProcessingContextPort = 9090;

    const char* const CONFIG_FILE_NAME = "configuration.json";

    /** Parse the config file and return the resulting rapidjson document.
     *  The filename of the config file is taken from the given environment variable.
     *  If that is not set or does not exist, fall back to the given file location and default
     *  file name. If that is also not found, return an empty document.
     */
    rapidjson::Document parseConfigFile(const std::string& configFile)
    {
        const std::string fileContent = FileHelper::readFileAsString(configFile);
        rapidjson::Document configuration;
        configuration.Parse(fileContent);

        if (configuration.HasParseError())
        {
            LOG(ERROR) << "JSON parse error: " << rapidjson::GetParseError_En(configuration.GetParseError());
        }

        return configuration;
    }


    rapidjson::Document parseConfigFile (
        const char* filePathVariable, const std::string& configLocation)
    {
        const auto configFileName = Environment::get(filePathVariable);
        if (configFileName.has_value())
        {
            // do not catch exception, if the environment variable is set the file must exist
            return parseConfigFile(configFileName.value());
        }

        std::vector<std::string> pathsToTry = {configLocation + "/" + CONFIG_FILE_NAME};
        // home variable may not be set (e.g. on Android)
        const auto home = Environment::get("HOME");
        if (home.has_value())
        {
            pathsToTry.emplace_back(home.value() + "/" + CONFIG_FILE_NAME);
        }

        for (const auto& configFile : pathsToTry)
        {
            try
            {
                VLOG(3) << "looking for config file " << configFile;
                return parseConfigFile(configFile);
            }
            catch(...)
            {
                // we don't need to generate a warning, config file can be replaced by
                // environment variables completely
                VLOG(3) << "Not able to read the configuration file from " << configFile;
            }
        }

        LOG(WARNING) << "No configuration file was found!";

        return rapidjson::Document();
    }

    InvalidConfigurationException createMissingKeyException (const KeyNames key)
    {
        return InvalidConfigurationException(
            "value not defined",
            std::string(key.environmentVariable),
            std::string(key.jsonPath));
    }

    uint16_t determineServerPort()
    {
        const auto portStr = Environment::get(ConfigurationBase::ServerPortEnvVar.data());
        if (portStr.has_value())
        {
            size_t pos = 0;
            const int port = std::stoi(portStr.value(), &pos, 10);
            Expect(pos == portStr.value().size(),
                   "Invalid value for environment variable " + std::string(ConfigurationBase::ServerPortEnvVar));
            Expect(port > 0 && port <= static_cast<int>(std::numeric_limits<uint16_t>::max()), "Configured port number is out of range");
            return static_cast<uint16_t>(port);
        }
        return DefaultProcessingContextPort;
    }
}


ConfigurationBase::ConfigurationBase (const std::vector<KeyNames>& allKeyNames)
    : mDocument(parseConfigFile(ErpConstants::ConfigurationFileNameVariable, ErpConstants::ConfigurationFileNameDefault))
{
    auto serverHost = Environment::get(ServerHostEnvVar.data());
    Expect(serverHost.has_value(), std::string("Environment variable \"") + ServerHostEnvVar.data() + "\" must be set.");
    mServerHost = serverHost.value();
    mServerPort = determineServerPort();

    if(!mDocument.IsObject())
        return;

    const auto pathPrefixCommon = "/common";
    const auto pathPrefixHost = "/" + mServerHost;
    const auto pathPrefixHostPort = pathPrefixHost + "/" + std::to_string(mServerPort);

    for(const auto& keyNames : allKeyNames)
    {
        const auto jsonPath = std::string(keyNames.jsonPath);

        if(lookupKey(pathPrefixHostPort, jsonPath))
            continue;

        if(lookupKey(pathPrefixHost, jsonPath))
            continue;

        lookupKey(pathPrefixCommon, jsonPath);
   }
}

const std::string& ConfigurationBase::serverHost() const
{
    return mServerHost;
}

uint16_t ConfigurationBase::serverPort() const
{
    return mServerPort;
}

bool ConfigurationBase::lookupKey(const std::string& pathPrefix, const std::string& jsonKey)
{
    const auto* jsonValue = rapidjson::Pointer(pathPrefix + jsonKey).Get(mDocument);
    if(jsonValue != nullptr)
    {
        Expect3(mValuesByKey.insert(std::make_pair(jsonKey, jsonValue)).second,
                "JSON path " + jsonKey + " already used", std::logic_error);
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------------------------------

template<>
std::map<ConfigurationKey,KeyNames> ConfigurationKeyNamesTemplate<ConfigurationKey>::mNamesByKey = {
    {ConfigurationKey::DEBUG_ENABLE_HSM_MOCK,                 {"DEBUG_ENABLE_HSM_MOCK",             "/debug/enable-hsm-mock"}},
    {ConfigurationKey::C_FD_SIG_ERP,                          {"ERP_C_FD_SIG_ERP",                  "/erp/c.fd.sig-erp"}},
    {ConfigurationKey::ECIES_CERTIFICATE,                     {"ERP_ECIES_CERTIFICATE",             "/erp/ecies-certificate"}},
    {ConfigurationKey::ENROLMENT_SERVER_PORT,                 {"ERP_ENROLMENT_SERVER_PORT",         "/erp/enrolment/server/port"}},
    {ConfigurationKey::ENROLMENT_ACTIVATE_FOR_PORT,           {"ERP_ENROLMENT_ACTIVATE_FOR_PORT",   "/erp/enrolment/server/activateForPort"}},
    {ConfigurationKey::ENROLMENT_API_CREDENTIALS,             {"ERP_ENROLMENT_API_CREDENTIALS",     "/erp/enrolment/api/credentials"}},
    {ConfigurationKey::DEBUG_DISABLE_ENROLMENT_API_AUTH,      {"DEBUG_DISABLE_ENROLMENT_API_AUTH",  "/debug/disable-enrolment-api-auth"}},
    {ConfigurationKey::ENTROPY_FETCH_INTERVAL_SECONDS,        {"ERP_ENTROPY_FETCH_INTERVAL_SECONDS","/erp/fhir/structure-files/entropy_fetch_interval_seconds"}},
    {ConfigurationKey::FHIR_STRUCTURE_DEFINITIONS,            {"ERP_FHIR_STRUCTURE_DEFINITIONS",    "/erp/fhir/structure-files"}},
    {ConfigurationKey::IDP_REGISTERED_FD_URI,                 {"ERP_IDP_REGISTERED_FD_URI",         "/erp/idp/registeredFdUri"}},
    {ConfigurationKey::IDP_CERTIFICATE_MAX_AGE_HOURS,         {"ERP_IDP_CERTIFICATE_MAX_AGE_HOURS", "/erp/idp/certificateMaxAgeHours"}},
    {ConfigurationKey::IDP_UPDATE_ENDPOINT,                   {"ERP_IDP_UPDATE_ENDPOINT",           "/erp/idp/updateEndpoint"}},
    {ConfigurationKey::IDP_UPDATE_ENDPOINT_SSL_ROOT_CA_PATH,  {"ERP_IDP_UPDATE_ENDPOINT_SSL_ROOT_CA_PATH",           "/erp/idp/sslRootCaPath"}},
    {ConfigurationKey::IDP_UPDATE_INTERVAL_MINUTES,           {"ERP_IDP_UPDATE_INTERVAL_MINUTES",   "/erp/idp/updateIntervalMinutes"}},
    {ConfigurationKey::OCSP_NON_QES_GRACE_PERIOD,             {"ERP_OCSP_NON_QES_GRACE_PERIOD",     "/erp/ocsp/gracePeriodNonQes"}},
    {ConfigurationKey::OCSP_QES_GRACE_PERIOD,                 {"ERP_OCSP_QES_GRACE_PERIOD",         "/erp/ocsp/gracePeriodQes"}},
    {ConfigurationKey::SERVER_THREAD_COUNT,                   {"ERP_SERVER_THREAD_COUNT",           "/erp/server/thread-count"}},
    {ConfigurationKey::SERVER_CERTIFICATE,                    {"ERP_SERVER_CERTIFICATE",            "/erp/server/certificate"}},
    {ConfigurationKey::SERVER_PRIVATE_KEY,                    {"ERP_SERVER_PRIVATE_KEY",            "/erp/server/certificateKey"}},
    {ConfigurationKey::SERVER_REQUEST_PATH,                   {"ERP_SERVER_REQUEST_PATH",           "/erp/server/requestPath"}},
    {ConfigurationKey::DEBUG_DISABLE_PROXY_AUTHENTICATION,    {"DEBUG_DISABLE_PROXY_AUTHENTICATION", "/debug/disable-proxy-authentication"}},
    {ConfigurationKey::SERVER_PROXY_CERTIFICATE,              {"ERP_SERVER_PROXY_CERTIFICATE",      "/erp/server/proxy/certificate"}},
    {ConfigurationKey::SERVER_KEEP_ALIVE,                     {"ERP_SERVER_KEEP_ALIVE",             "/erp/server/keep-alive"}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_ENTLASSREZEPT_VALIDITY_WD,{"ERP_SERVICE_TASK_ACTIVATE_ENTLASSREZEPT_VALIDITY_WD","/erp/service/task/activate/entlassRezeptValidityInWorkDays"}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_HOLIDAYS,        {"ERP_SERVICE_TASK_ACTIVATE_HOLIDAYS","/erp/service/task/activate/holidays"}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_EASTER_CSV,      {"ERP_SERVICE_TASK_ACTIVATE_EASTER_CSV","/erp/service/task/activate/easterCsv"}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_KBV_VALIDATION,  {"ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION","/erp/service/task/activate/kbvValidation"}},
    {ConfigurationKey::SERVICE_COMMUNICATION_MAX_MESSAGES,    {"ERP_SERVICE_COMMUNICATION_MAX_MESSAGES","/erp/service/communication/maxMessageCount"}},
    {ConfigurationKey::PCR_SET,                               {"ERP_PCR_SET",                       "/erp/service/pcr-set"}},
    {ConfigurationKey::POSTGRES_HOST,                         {"ERP_POSTGRES_HOST",                 "/erp/postgres/host"}},
    {ConfigurationKey::POSTGRES_PORT,                         {"ERP_POSTGRES_PORT",                 "/erp/postgres/port"}},
    {ConfigurationKey::POSTGRES_USER,                         {"ERP_POSTGRES_USER",                 "/erp/postgres/user"}},
    {ConfigurationKey::POSTGRES_PASSWORD,                     {"ERP_POSTGRES_PASSWORD",             "/erp/postgres/password"}},
    {ConfigurationKey::POSTGRES_DATABASE,                     {"ERP_POSTGRES_DATABASE",             "/erp/postgres/database"}},
    {ConfigurationKey::POSTGRES_CERTIFICATE,                  {"ERP_POSTGRES_CERTIFICATE",          "/erp/postgres/certificate"}},
    {ConfigurationKey::POSTGRES_SSL_ROOT_CERTIFICATE_PATH,    {"ERP_POSTGRES_CERTIFICATE_PATH",     "/erp/postgres/certificatePath"}},
    {ConfigurationKey::POSTGRES_SSL_CERTIFICATE_PATH,         {"ERP_POSTGRES_SSL_CERTIFICATE_PATH", "/erp/postgres/sslCertificatePath"}},
    {ConfigurationKey::POSTGRES_SSL_KEY_PATH,                 {"ERP_POSTGRES_SSL_KEY_PATH",         "/erp/postgres/sslKeyPath"}},
    {ConfigurationKey::POSTGRES_USESSL,                       {"ERP_POSTGRES_USESSL",               "/erp/postgres/useSsl"}},
    {ConfigurationKey::POSTGRES_CONNECT_TIMEOUT_SECONDS,      {"ERP_POSTGRES_CONNECT_TIMEOUT_SECONDS","/erp/postgres/connectTimeoutSeconds"}},
    {ConfigurationKey::POSTGRES_ENABLE_SCRAM_AUTHENTICATION,  {"ERP_POSTGRES_ENABLE_SCRAM_AUTHENTICATION","/erp/postgres/enableScramAuthentication"}},
    {ConfigurationKey::POSTGRES_TCP_USER_TIMEOUT_MS,          {"ERP_POSTGRES_TCP_USER_TIMEOUT_MS",      "/erp/postgres/tcpUserTimeoutMs"}},
    {ConfigurationKey::POSTGRES_KEEPALIVES_IDLE_SEC,          {"ERP_POSTGRES_KEEPALIVES_IDLE_SEC",      "/erp/postgres/keepalivesIdleSec"}},
    {ConfigurationKey::POSTGRES_KEEPALIVES_INTERVAL_SEC,      {"ERP_POSTGRES_KEEPALIVES_INTERVAL_SEC",  "/erp/postgres/keepalivesIntervalSec"}},
    {ConfigurationKey::POSTGRES_KEEPALIVES_COUNT,             {"ERP_POSTGRES_KEEPALIVES_COUNT",         "/erp/postgres/keepalivesCount"}},
    {ConfigurationKey::PUBLIC_E_PRESCRIPTION_SERVICE_URL,     {"ERP_E_PRESCRIPTION_SERVICE_URL",    "/erp/publicEPrescriptionServiceUrl"}},
    {ConfigurationKey::REGISTRATION_HEARTBEAT_INTERVAL_SEC,   {"ERP_REGISTRATION_HEARTBEAT_INTERVAL_SEC", "/erp/registration/heartbeatIntervalSec"}},
    {ConfigurationKey::TSL_TI_OCSP_PROXY_URL,                 {"ERP_TSL_TI_OCSP_PROXY_URL",         "/erp/tsl/tiOcspProxyUrl"}},
    {ConfigurationKey::TSL_FRAMEWORK_SSL_ROOT_CA_PATH,        {"ERP_TSL_FRAMEWORK_SSL_ROOT_CA_PATH","/erp/tsl/sslRootCaPath"}},
    {ConfigurationKey::TSL_INITIAL_DOWNLOAD_URL,              {"ERP_TSL_INITIAL_DOWNLOAD_URL",      "/erp/tsl/initialDownloadUrl"}},
    {ConfigurationKey::TSL_INITIAL_CA_DER_PATH,               {"ERP_TSL_INITIAL_CA_DER_PATH",       "/erp/tsl/initialCaDerPath"}},
    {ConfigurationKey::TSL_INITIAL_CA_DER_PATH_NEW,           {"ERP_TSL_INITIAL_CA_DER_PATH_NEW",   "/erp/tsl/initialCaDerPathNew"}},
    {ConfigurationKey::TSL_INITIAL_CA_DER_PATH_NEW_START,     {"ERP_TSL_INITIAL_CA_DER_PATH_NEW_START", "/erp/tsl/initialCaDerPathStart"}},
    {ConfigurationKey::TSL_REFRESH_INTERVAL,                  {"ERP_TSL_REFRESH_INTERVAL",          "/erp/tsl/refreshInterval"}},
    {ConfigurationKey::TSL_DOWNLOAD_CIPHERS,                  {"ERP_TSL_DOWNLOAD_CIPHERS",          "/erp/tsl/downloadCiphers"}},
    {ConfigurationKey::DEBUG_DISABLE_REGISTRATION,            {"DEBUG_DISABLE_REGISTRATION",        "/debug/disable-registration"}},
    {ConfigurationKey::JSON_META_SCHEMA,                      {"ERP_JSON_META_SCHEMA",              "/erp/schema/json-meta-schema"}},
    {ConfigurationKey::JSON_SCHEMA,                           {"ERP_JSON_SCHEMA",                   "/erp/schema/json-schema"}},
    {ConfigurationKey::REDIS_DATABASE,                        {"ERP_REDIS_DATABASE",                "/erp/redis/database"}},
    {ConfigurationKey::REDIS_USER,                            {"ERP_REDIS_USERNAME",                "/erp/redis/user"}},
    {ConfigurationKey::REDIS_PASSWORD,                        {"ERP_REDIS_PASSWORD",                "/erp/redis/password"}},
    {ConfigurationKey::REDIS_HOST,                            {"ERP_REDIS_HOST",                    "/erp/redis/host"}},
    {ConfigurationKey::REDIS_PORT,                            {"ERP_REDIS_PORT",                    "/erp/redis/port"}},
    {ConfigurationKey::REDIS_CERTIFICATE_PATH,                {"ERP_REDIS_CERTIFICATE_PATH",        "/erp/redis/certificatePath"}},
    {ConfigurationKey::REDIS_CONNECTION_TIMEOUT,              {"ERP_REDIS_CONNECTION_TIMEOUT",      "/erp/redis/connectionTimeout"}},
    {ConfigurationKey::REDIS_CONNECTIONPOOL_SIZE,             {"ERP_REDIS_CONNECTIONPOOL_SIZE",     "/erp/redis/connectionPoolSize"}},
    {ConfigurationKey::DEBUG_DISABLE_DOS_CHECK,               {"DEBUG_DISABLE_DOS_CHECK",           "/debug/disable-dos-check"}},
    {ConfigurationKey::TOKEN_ULIMIT_CALLS,                    {"ERP_TOKEN_ULIMIT_CALLS",            "/erp/server/token/ulimitCalls"}},
    {ConfigurationKey::TOKEN_ULIMIT_TIMESPAN_MS,              {"ERP_TOKEN_ULIMIT_TIMESPAN_MS",      "/erp/server/token/ulimitTimespanMS"}},
    {ConfigurationKey::XML_SCHEMA,                            {"ERP_XML_SCHEMA",                    "/erp/schema/xml-schema"}},
    {ConfigurationKey::HSM_DEVICE,                            {"ERP_HSM_DEVICE",                    "/erp/hsm/device"}},
    {ConfigurationKey::HSM_MAX_SESSION_COUNT,                 {"ERP_HSM_MAX_SESSION_COUNT",         "/erp/hsm/max-session-count"}},
    {ConfigurationKey::HSM_WORK_USERNAME,                     {"ERP_HSM_WORK_USERNAME",             "/erp/hsm/work-username"}},
    {ConfigurationKey::HSM_WORK_PASSWORD,                     {"ERP_HSM_WORK_PASSWORD",             "/erp/hsm/work-password"}},
    {ConfigurationKey::HSM_WORK_KEYSPEC,                      {"ERP_HSM_WORK_KEYSPEC",              "/erp/hsm/work-keyspec"}},
    {ConfigurationKey::HSM_SETUP_USERNAME,                    {"ERP_HSM_SETUP_USERNAME",            "/erp/hsm/setup-username"}},
    {ConfigurationKey::HSM_SETUP_PASSWORD,                    {"ERP_HSM_SETUP_PASSWORD",            "/erp/hsm/setup-password"}},
    {ConfigurationKey::HSM_SETUP_KEYSPEC,                     {"ERP_HSM_SETUP_KEYSPEC",             "/erp/hsm/setup-keyspec"}},
    {ConfigurationKey::HSM_CONNECT_TIMEOUT_SECONDS,           {"ERP_HSM_CONNECT_TIMEOUT_SECONDS",   "/erp/hsm/connect-timeout-seconds"}},
    {ConfigurationKey::HSM_READ_TIMEOUT_SECONDS,              {"ERP_HSM_READ_TIMEOUT_SECONDS",      "/erp/hsm/read-timeout-seconds"}},
    {ConfigurationKey::HSM_IDLE_TIMEOUT_SECONDS,              {"ERP_HSM_IDLE_TIMEOUT_SECONDS",      "/erp/hsm/idle-timeout-seconds"}},
    {ConfigurationKey::HSM_RECONNECT_INTERVAL_SECONDS,        {"ERP_HSM_RECONNECT_INTERVAL_SECONDS","/erp/hsm/reconnect-interval-seconds"}},
    {ConfigurationKey::DEPRECATED_HSM_USERNAME,               {"ERP_HSM_USERNAME",                  "/erp/hsm/username"}},
    {ConfigurationKey::DEPRECATED_HSM_PASSWORD,               {"ERP_HSM_PASWORD",                   "/erp/hsm/password"}},
    {ConfigurationKey::TEE_TOKEN_UPDATE_SECONDS,              {"ERP_TEE_TOKEN_UPDATE_SECONDS",      "/erp/hsm/tee-token/update-seconds"}},
    {ConfigurationKey::TEE_TOKEN_RETRY_SECONDS,               {"ERP_TEE_TOKEN_RETRY_SECONDS",       "/erp/hsm/tee-token/retry-seconds"}},
    {ConfigurationKey::ZSTD_DICTIONARY_DIR,                   {"ERP_ZSTD_DICTIONARY_DIR",           "/erp/compression/zstd/dictionary-dir"}},
    {ConfigurationKey::HTTPCLIENT_CONNECT_TIMEOUT_SECONDS,    {"ERP_HTTPCLIENT_CONNECT_TIMEOUT_SECONDS","/erp/httpClientConnectTimeoutSeconds"}}
};

// -----------------------------------------------------------------------------------------------------

std::optional<int> ConfigurationBase::getIntValueInternal(KeyNames key) const
{
    const auto value = getStringValueInternal(key);
    if (value.has_value())
    {
        try
        {
            size_t pos = 0;
            auto asInt = stoi(value.value(), &pos);
            if (pos == value->size())
            {
                return asInt;
            }
        }
        catch(const std::logic_error&)
        {
        }
        throw InvalidConfigurationException(
            "Configuration: can not convert '" + value.value() + "' to integer",
            std::string(key.environmentVariable),
            std::string(key.jsonPath));
    }
    else
        return {};
}

std::optional<bool> ConfigurationBase::getBoolValueInternal(KeyNames key) const
{
    const auto value = getStringValueInternal(key);
    if (value.has_value())
        return String::toBool(value.value());
    else
        return {};
}

int ConfigurationBase::getOptionalIntValue (const KeyNames key, const int defaultValue) const
{
    return getIntValueInternal(key).value_or(defaultValue);
}


int ConfigurationBase::getIntValue (const KeyNames key) const
{
    const auto value = getIntValueInternal(key);
    if (value.has_value())
        return value.value();
    else
        throw createMissingKeyException(key);
}

std::string ConfigurationBase::getStringValue (const KeyNames key) const
{
    const auto value = getStringValueInternal(key);
    if (value.has_value())
        return value.value();
    else
        throw createMissingKeyException(key);
}


std::string ConfigurationBase::getOptionalStringValue (const KeyNames key, const std::string& defaultValue) const
{
    const auto value = getStringValueInternal(key);
    if (value.has_value())
        return value.value();
    else
        return defaultValue;
}


std::optional<std::string> ConfigurationBase::getOptionalStringValue (const KeyNames key) const
{
    const auto value = getStringValueInternal(key);
    if (value.has_value())
        return value.value();
    else
        return std::nullopt;
}


SafeString ConfigurationBase::getSafeStringValue (const KeyNames key) const
{
    auto value = getSafeStringValueInternal(key);
    if (value.has_value())
        return std::move(value.value());
    else
        throw createMissingKeyException(key);
}


SafeString ConfigurationBase::getOptionalSafeStringValue (const KeyNames key, SafeString defaultValue) const
{
    auto value = getSafeStringValueInternal(key);
    if (value.has_value())
        return std::move(value.value());
    else
        return defaultValue;
}


std::filesystem::path ConfigurationBase::getPathValue(const KeyNames key) const
{
    namespace fs = std::filesystem;
    fs::path path(getStringValue(key));
    return path.lexically_normal();
}

bool ConfigurationBase::getBoolValue (const KeyNames key) const
{
    const auto value = getBoolValueInternal(key);
    if (value.has_value())
        return value.value();
    else
        throw createMissingKeyException(key);
}


bool ConfigurationBase::getOptionalBoolValue (KeyNames key, const bool defaultValue) const
{
    const auto value = getBoolValueInternal(key);
    if (value.has_value())
        return value.value();
    else
        return defaultValue;
}

std::vector<std::string> ConfigurationBase::getArrayInternal(KeyNames key) const
{
    using namespace std::string_literals;
    std::vector<std::string> result;
    const auto& value = Environment::get(std::string{key.environmentVariable});
    if (value)
    {
        result = String::split(value.value(), ';');
        return result;
    }

    const auto jsonValueIter = mValuesByKey.find(std::string(key.jsonPath));
    if(jsonValueIter != mValuesByKey.end())
    {
        const auto* jsonValue = jsonValueIter->second;
        Expect3(jsonValue != nullptr, "JSON value must not be nullptr", std::logic_error);
        Expect3(jsonValue->IsArray(), "JSON value must be array", std::logic_error);
        const auto& jsonArray = jsonValue->GetArray();
        result.reserve(jsonArray.Size());
        for (const auto& val: jsonArray)
        {
            Expect3(val.IsString(), "Configuration values in array must be strings: "s.append(key.jsonPath) , std::logic_error);
            result.emplace_back(val.GetString());
        }
        return result;
    }
    throw createMissingKeyException(key);
}

std::optional<std::string> ConfigurationBase::getStringValueInternal (KeyNames key) const
{
    const auto& value = Environment::get(key.environmentVariable.data());
    if (value)
        return value;

    // Could not find the value in the environment. Now try the configuration file.
    const auto jsonValueIter = mValuesByKey.find(std::string(key.jsonPath));
    if(jsonValueIter != mValuesByKey.end())
    {
        const auto* jsonValue = jsonValueIter->second;
        Expect3(jsonValue != nullptr, "JSON value must not be nullptr", std::logic_error);
        Expect3(jsonValue->IsString(), "JSON value must be string", std::logic_error);
        return jsonValue->GetString();
    }

    // Not found in the configuration file either.
    return {};
}


std::optional<SafeString> ConfigurationBase::getSafeStringValueInternal (const KeyNames key) const
{
    const char* value = Environment::getCharacterPointer(key.environmentVariable.data());
    if (value != nullptr)
        return SafeString(value);

    // Could not find the value in the environment. Now try the configuration file.
    const auto jsonValueIter = mValuesByKey.find(std::string(key.jsonPath));
    if(jsonValueIter != mValuesByKey.end())
    {
        const auto* jsonValue = jsonValueIter->second;
        Expect3(jsonValue != nullptr, "JSON value must not be nullptr", std::logic_error);
        Expect3(jsonValue->IsString(), "JSON value must be string", std::logic_error);
        return SafeString(jsonValue->GetString());
    }

    // Not found in the configuration file either.
    return std::nullopt;
}
