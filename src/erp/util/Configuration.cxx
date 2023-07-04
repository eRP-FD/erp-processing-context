/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/Configuration.hxx"
#include "erp/ErpConstants.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/GLog.hxx"
#include "erp/util/InvalidConfigurationException.hxx"
#include "erp/util/String.hxx"
#include "erp/validation/XmlValidator.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"

#include <stdexcept>
#include <string>
#include <unordered_set>


// Compile the rapidjson assert so that it calls Expects() and will throw an exception instead of just aborting (crashing)
// the application.
#undef RAPIDJSON_ASSERT
#define RAPIDJSON_ASSERT(x) Expect(x, "rapidjson error")
#include <boost/algorithm/string/predicate.hpp>
#include <rapidjson/error/en.h>
#include <rapidjson/pointer.h>
#include <rapidjson/prettywriter.h>
#include <regex>


namespace {
    constexpr uint16_t DefaultProcessingContextPort = 9090;

    void mergeConfig(rapidjson::Value& dst, const rapidjson::Value& src, rapidjson::Document::AllocatorType& allocator) // NOLINT(misc-no-recursion)
    {
        for (auto srcIt = src.MemberBegin(), end = src.MemberEnd(); srcIt != end; ++srcIt)
        {
            auto dstIt = dst.FindMember(srcIt->name);
            if (dstIt == dst.MemberEnd())
            {
                rapidjson::Value dstName;
                dstName.CopyFrom(srcIt->name, allocator);
                rapidjson::Value dstVal;
                dstVal.CopyFrom(srcIt->value, allocator);
                dst.AddMember(dstName, dstVal, allocator);
            }
            else
            {
                Expect3(srcIt->value.GetType() == dstIt->value.GetType(), "configuration type missmatch", std::logic_error);
                if (srcIt->value.IsArray())
                {
                    for (const auto* arrayIt = srcIt->value.Begin(); arrayIt != srcIt->value.End(); ++arrayIt)
                    {
                        rapidjson::Value dstVal;
                        dstVal.CopyFrom(*arrayIt, allocator);
                        dstIt->value.PushBack(dstVal, allocator);
                    }
                }
                else if (srcIt->value.IsObject())
                {
                    mergeConfig(dstIt->value, srcIt->value, allocator);
                }
                else
                {
                    dstIt->value.CopyFrom(srcIt->value, allocator);
                }
            }
        }
    }

    /** Parse the config file and return the resulting rapidjson document.
     *  The filename of the config file is taken from the given environment variable.
     *  If that is not set or does not exist, fall back to the given file location and default
     *  file name. If that is also not found, return an empty document.
     */
    void parseConfigFile(const std::string& configFile, rapidjson::Document& configuration)
    {
        using namespace std::string_literals;
        const std::string fileContent = FileHelper::readFileAsString(configFile);
        rapidjson::Document tmp;
        tmp.Parse(fileContent);
        Expect3(!tmp.HasParseError(), "JSON parse error: "s.append(rapidjson::GetParseError_En(tmp.GetParseError())),
                std::logic_error);
        mergeConfig(configuration, tmp, configuration.GetAllocator());
    }


    std::map<std::string, std::filesystem::path> locateConfigFiles(const std::vector<std::filesystem::path>& pathsToTry)
    {
        const std::regex configRegex(ErpConstants::ConfigurationFileSearchPatternDefault);

        std::map<std::string, std::filesystem::path> sortedFiles;

        for (const auto& path : pathsToTry)
        {
            if (path.empty() || ! std::filesystem::exists(path))
                continue;

            for (const auto& candidate : std::filesystem::directory_iterator(std::filesystem::path(path)))
            {
                const auto filename = candidate.path().filename().string();
                if (candidate.is_regular_file() && std::regex_match(filename, configRegex))
                {
                    sortedFiles.emplace(filename, candidate.path());
                }
            }
        }
        return sortedFiles;
    }

    [[maybe_unused]]
    std::string dumpConfig(const rapidjson::Document& configuration)
    {
        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        configuration.Accept(writer);
        return buffer.GetString();
    }


    rapidjson::Document parseConfigFile (
        const char* filePathVariable, const std::string& configLocation)
    {
        const auto configFileName = Environment::get(filePathVariable);
        rapidjson::Document configuration;
        configuration.SetObject();
        int loaded = 0;
        if (configFileName.has_value())
        {
            // MODE 1: load exactly one config file as specified by environment override
            // do not catch exception, if the environment variable is set the file must exist
            TLOG(INFO) << "loading config file " << configFileName.value();
            parseConfigFile(configFileName.value(), configuration);
            ++loaded;
        }
        else
        {
            // MODE 2: load multiple config files in a specific order, e.g 01_production_config.json, 02_development.config.json
            std::vector<std::filesystem::path> pathsToTry = {configLocation};
            // home variable may not be set (e.g. on Android)
            const auto home = Environment::get("HOME");
            if (home.has_value())
            {
                pathsToTry.emplace_back(home.value());
            }

            const auto sortedFiles = locateConfigFiles(pathsToTry);
            for (const auto& configFile : sortedFiles)
            {
                try
                {
                    TLOG(INFO) << "loading config file " << configFile.second;
                    parseConfigFile(configFile.second.string(), configuration);
                    ++loaded;
                }
                catch (...)
                {
                    TLOG(ERROR) << "Not able to read the configuration file from " << configFile.second;
                    throw;
                }
            }
        }

        if (loaded == 0)
        {
            TLOG(ERROR) << "no configuration file loaded!";
        }
        else
        {
            TLOG(INFO) << "loaded " << loaded << " configuration file" << (loaded > 1?"s":"");
        }

        return configuration;
    }

    ExceptionWrapper<InvalidConfigurationException> createMissingKeyException(const KeyNames key,
                                                                              const FileNameAndLineNumber& location)
    {
        return ExceptionWrapper<InvalidConfigurationException>::create(location,
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
    : mDocument(parseConfigFile(ErpConstants::ConfigurationFileNameVariable, ErpConstants::ConfigurationFileSearchPathDefault))
    , mServerPort(determineServerPort())
{
    auto serverHost = Environment::get(ServerHostEnvVar.data());
    Expect(serverHost.has_value(),
           std::string("Environment variable \"") + ServerHostEnvVar.data() + "\" must be set.");
    mServerHost = serverHost.value();

    if (! mDocument.IsObject())
        return;

    const auto* pathPrefixCommon = "/common";
    const auto pathPrefixHost = "/" + mServerHost;
    const auto pathPrefixHostPort = pathPrefixHost + "/" + std::to_string(mServerPort);

    for (const auto& keyNames : allKeyNames)
    {
        const auto jsonPath = std::string(keyNames.jsonPath);

        if (lookupKey(pathPrefixHostPort, jsonPath))
            continue;

        if (lookupKey(pathPrefixHost, jsonPath))
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

OpsConfigKeyNames::OpsConfigKeyNames()
{
    using Flags = KeyNames::ConfigurationKeyFlags;
    // clang-format off
    mNamesByKey.insert({
    {ConfigurationKey::C_FD_SIG_ERP                                   , {"ERP_C_FD_SIG_ERP"                                   , "/erp/c.fd.sig-erp"}},
    {ConfigurationKey::C_FD_SIG_ERP_VALIDATION_INTERVAL               , {"ERP_C_FD_SIG_ERP_VALIDATION_INTERVAL"               , "/erp/c.fd.sig-erp-validation"}},
    {ConfigurationKey::ECIES_CERTIFICATE                              , {"ERP_ECIES_CERTIFICATE"                              , "/erp/ecies-certificate"}},
    {ConfigurationKey::ENROLMENT_SERVER_PORT                          , {"ERP_ENROLMENT_SERVER_PORT"                          , "/erp/enrolment/server/port"}},
    {ConfigurationKey::ENROLMENT_ACTIVATE_FOR_PORT                    , {"ERP_ENROLMENT_ACTIVATE_FOR_PORT"                    , "/erp/enrolment/server/activateForPort"}},
    {ConfigurationKey::ENROLMENT_API_CREDENTIALS                      , {"ERP_ENROLMENT_API_CREDENTIALS"                      , "/erp/enrolment/api/credentials", Flags::credential}},
    {ConfigurationKey::ENTROPY_FETCH_INTERVAL_SECONDS                 , {"ERP_ENTROPY_FETCH_INTERVAL_SECONDS"                 , "/erp/entropy_fetch_interval_seconds"}},
    {ConfigurationKey::IDP_REGISTERED_FD_URI                          , {"ERP_IDP_REGISTERED_FD_URI"                          , "/erp/idp/registeredFdUri"}},
    {ConfigurationKey::IDP_CERTIFICATE_MAX_AGE_HOURS                  , {"ERP_IDP_CERTIFICATE_MAX_AGE_HOURS"                  , "/erp/idp/certificateMaxAgeHours"}},
    {ConfigurationKey::IDP_UPDATE_ENDPOINT                            , {"ERP_IDP_UPDATE_ENDPOINT"                            , "/erp/idp/updateEndpoint"}},
    {ConfigurationKey::IDP_UPDATE_ENDPOINT_SSL_ROOT_CA_PATH           , {"ERP_IDP_UPDATE_ENDPOINT_SSL_ROOT_CA_PATH"           , "/erp/idp/sslRootCaPath"}},
    {ConfigurationKey::IDP_UPDATE_INTERVAL_MINUTES                    , {"ERP_IDP_UPDATE_INTERVAL_MINUTES"                    , "/erp/idp/updateIntervalMinutes"}},
    {ConfigurationKey::IDP_NO_VALID_CERTIFICATE_UPDATE_INTERVAL_SECONDS, {"ERP_IDP_NO_VALID_CERTIFICATE_UPDATE_INTERVAL_SECONDS", "/erp/idp/noValidCertificateUpdateIntervalSeconds"}},
    {ConfigurationKey::OCSP_C_FD_SIG_ERP_GRACE_PERIOD                 , {"ERP_OCSP_C_FD_SIG_ERP_GRACE_PERIOD"                 , "/erp/ocsp/gracePeriodCFdSigErp"}},
    {ConfigurationKey::OCSP_SMC_B_OSIG_GRACE_PERIOD                   , {"ERP_OCSP_SMC_B_OSIG_GRACE_PERIOD"                   , "/erp/ocsp/gracePeriodSmcBOsig"}},
    {ConfigurationKey::OCSP_NON_QES_GRACE_PERIOD                      , {"ERP_OCSP_NON_QES_GRACE_PERIOD"                      , "/erp/ocsp/gracePeriodNonQes"}},
    {ConfigurationKey::OCSP_QES_GRACE_PERIOD                          , {"ERP_OCSP_QES_GRACE_PERIOD"                          , "/erp/ocsp/gracePeriodQes"}},
    {ConfigurationKey::SERVER_THREAD_COUNT                            , {"ERP_SERVER_THREAD_COUNT"                            , "/erp/server/thread-count"}},
    {ConfigurationKey::SERVER_CERTIFICATE                             , {"ERP_SERVER_CERTIFICATE"                             , "/erp/server/certificate"}},
    {ConfigurationKey::SERVER_PRIVATE_KEY                             , {"ERP_SERVER_PRIVATE_KEY"                             , "/erp/server/certificateKey", Flags::credential}},
    {ConfigurationKey::SERVER_REQUEST_PATH                            , {"ERP_SERVER_REQUEST_PATH"                            , "/erp/server/requestPath"}},
    {ConfigurationKey::SERVER_PROXY_CERTIFICATE                       , {"ERP_SERVER_PROXY_CERTIFICATE"                       , "/erp/server/proxy/certificate"}},
    {ConfigurationKey::SERVICE_OLD_PROFILE_GENERIC_VALIDATION_MODE    , {"ERP_SERVICE_OLD_PROFILE_GENERIC_VALIDATION_MODE"    , "/erp/fhir-profile-old/fhir-validation/mode"}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_ENTLASSREZEPT_VALIDITY_WD, {"ERP_SERVICE_TASK_ACTIVATE_ENTLASSREZEPT_VALIDITY_WD", "/erp/service/task/activate/entlassRezeptValidityInWorkDays"}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_HOLIDAYS                 , {"ERP_SERVICE_TASK_ACTIVATE_HOLIDAYS"                 , "/erp/service/task/activate/holidays", Flags::array}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_EASTER_CSV               , {"ERP_SERVICE_TASK_ACTIVATE_EASTER_CSV"               , "/erp/service/task/activate/easterCsv"}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION, {"ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION", "/erp/service/task/activate/kbvValidationOnUnknownExtension"}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_KBV_VALIDATION_NON_LITERAL_AUTHOR_REF, {"ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_NON_LITERAL_AUTHOR_REF", "/erp/service/task/activate/kbvValidationNonLiteralAuthorRef"}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_AUTHORED_ON_MUST_EQUAL_SIGNING_DATE, {"ERP_SERVICE_TASK_ACTIVATE_AUTHORED_ON_MUST_EQUAL_SIGNING_DATE", "/erp/service/task/activate/authoredOnMustEqualSigningDate"}},
    {ConfigurationKey::SERVICE_COMMUNICATION_MAX_MESSAGES             , {"ERP_SERVICE_COMMUNICATION_MAX_MESSAGES"             , "/erp/service/communication/maxMessageCount"}},
    {ConfigurationKey::SERVICE_SUBSCRIPTION_SIGNING_KEY               , {"ERP_SERVICE_SUBSCRIPTION_SIGNING_KEY"               , "/erp/service/subscription/signingKey", Flags::credential}},
    {ConfigurationKey::PCR_SET                                        , {"ERP_PCR_SET"                                        , "/erp/service/pcr-set"}},
    {ConfigurationKey::POSTGRES_HOST                                  , {"ERP_POSTGRES_HOST"                                  , "/erp/postgres/host"}},
    {ConfigurationKey::POSTGRES_PORT                                  , {"ERP_POSTGRES_PORT"                                  , "/erp/postgres/port"}},
    {ConfigurationKey::POSTGRES_USER                                  , {"ERP_POSTGRES_USER"                                  , "/erp/postgres/user"}},
    {ConfigurationKey::POSTGRES_PASSWORD                              , {"ERP_POSTGRES_PASSWORD"                              , "/erp/postgres/password", Flags::credential}},
    {ConfigurationKey::POSTGRES_DATABASE                              , {"ERP_POSTGRES_DATABASE"                              , "/erp/postgres/database"}},
    {ConfigurationKey::POSTGRES_CERTIFICATE                           , {"ERP_POSTGRES_CERTIFICATE"                           , "/erp/postgres/certificate"}},
    {ConfigurationKey::POSTGRES_SSL_ROOT_CERTIFICATE_PATH             , {"ERP_POSTGRES_CERTIFICATE_PATH"                      , "/erp/postgres/certificatePath"}},
    {ConfigurationKey::POSTGRES_SSL_CERTIFICATE_PATH                  , {"ERP_POSTGRES_SSL_CERTIFICATE_PATH"                  , "/erp/postgres/sslCertificatePath"}},
    {ConfigurationKey::POSTGRES_SSL_KEY_PATH                          , {"ERP_POSTGRES_SSL_KEY_PATH"                          , "/erp/postgres/sslKeyPath"}},
    {ConfigurationKey::POSTGRES_USESSL                                , {"ERP_POSTGRES_USESSL"                                , "/erp/postgres/useSsl"}},
    {ConfigurationKey::POSTGRES_CONNECT_TIMEOUT_SECONDS               , {"ERP_POSTGRES_CONNECT_TIMEOUT_SECONDS"               , "/erp/postgres/connectTimeoutSeconds"}},
    {ConfigurationKey::POSTGRES_ENABLE_SCRAM_AUTHENTICATION           , {"ERP_POSTGRES_ENABLE_SCRAM_AUTHENTICATION"           , "/erp/postgres/enableScramAuthentication"}},
    {ConfigurationKey::POSTGRES_TCP_USER_TIMEOUT_MS                   , {"ERP_POSTGRES_TCP_USER_TIMEOUT_MS"                   , "/erp/postgres/tcpUserTimeoutMs"}},
    {ConfigurationKey::POSTGRES_KEEPALIVES_IDLE_SEC                   , {"ERP_POSTGRES_KEEPALIVES_IDLE_SEC"                   , "/erp/postgres/keepalivesIdleSec"}},
    {ConfigurationKey::POSTGRES_KEEPALIVES_INTERVAL_SEC               , {"ERP_POSTGRES_KEEPALIVES_INTERVAL_SEC"               , "/erp/postgres/keepalivesIntervalSec"}},
    {ConfigurationKey::POSTGRES_KEEPALIVES_COUNT                      , {"ERP_POSTGRES_KEEPALIVES_COUNT"                      , "/erp/postgres/keepalivesCount"}},
    {ConfigurationKey::POSTGRES_TARGET_SESSION_ATTRS                  , {"ERP_POSTGRES_TARGET_SESSION_ATTRS"                  , "/erp/postgres/targetSessionAttrs"}},
    {ConfigurationKey::PUBLIC_E_PRESCRIPTION_SERVICE_URL              , {"ERP_E_PRESCRIPTION_SERVICE_URL"                     , "/erp/publicEPrescriptionServiceUrl"}},
    {ConfigurationKey::REGISTRATION_HEARTBEAT_INTERVAL_SEC            , {"ERP_REGISTRATION_HEARTBEAT_INTERVAL_SEC"            , "/erp/registration/heartbeatIntervalSec"}},
    {ConfigurationKey::TSL_TI_OCSP_PROXY_URL                          , {"ERP_TSL_TI_OCSP_PROXY_URL"                          , "/erp/tsl/tiOcspProxyUrl"}},
    {ConfigurationKey::TSL_FRAMEWORK_SSL_ROOT_CA_PATH                 , {"ERP_TSL_FRAMEWORK_SSL_ROOT_CA_PATH"                 , "/erp/tsl/sslRootCaPath"}},
    {ConfigurationKey::TSL_INITIAL_DOWNLOAD_URL                       , {"ERP_TSL_INITIAL_DOWNLOAD_URL"                       , "/erp/tsl/initialDownloadUrl"}},
    {ConfigurationKey::TSL_INITIAL_CA_DER_PATH                        , {"ERP_TSL_INITIAL_CA_DER_PATH"                        , "/erp/tsl/initialCaDerPath"}},
    {ConfigurationKey::TSL_INITIAL_CA_DER_PATH_NEW                    , {"ERP_TSL_INITIAL_CA_DER_PATH_NEW"                    , "/erp/tsl/initialCaDerPathNew"}},
    {ConfigurationKey::TSL_INITIAL_CA_DER_PATH_NEW_START              , {"ERP_TSL_INITIAL_CA_DER_PATH_NEW_START"              , "/erp/tsl/initialCaDerPathStart"}},
    {ConfigurationKey::TSL_REFRESH_INTERVAL                           , {"ERP_TSL_REFRESH_INTERVAL"                           , "/erp/tsl/refreshInterval"}},
    {ConfigurationKey::TSL_DOWNLOAD_CIPHERS                           , {"ERP_TSL_DOWNLOAD_CIPHERS"                           , "/erp/tsl/downloadCiphers"}},
    {ConfigurationKey::JSON_META_SCHEMA                               , {"ERP_JSON_META_SCHEMA"                               , "/erp/json-meta-schema"}},
    {ConfigurationKey::JSON_SCHEMA                                    , {"ERP_JSON_SCHEMA"                                    , "/erp/json-schema", Flags::array}},
    {ConfigurationKey::REDIS_DATABASE                                 , {"ERP_REDIS_DATABASE"                                 , "/erp/redis/database"}},
    {ConfigurationKey::REDIS_USER                                     , {"ERP_REDIS_USERNAME"                                 , "/erp/redis/user"}},
    {ConfigurationKey::REDIS_PASSWORD                                 , {"ERP_REDIS_PASSWORD"                                 , "/erp/redis/password", Flags::credential}},
    {ConfigurationKey::REDIS_HOST                                     , {"ERP_REDIS_HOST"                                     , "/erp/redis/host"}},
    {ConfigurationKey::REDIS_PORT                                     , {"ERP_REDIS_PORT"                                     , "/erp/redis/port"}},
    {ConfigurationKey::REDIS_CERTIFICATE_PATH                         , {"ERP_REDIS_CERTIFICATE_PATH"                         , "/erp/redis/certificatePath"}},
    {ConfigurationKey::REDIS_CONNECTION_TIMEOUT                       , {"ERP_REDIS_CONNECTION_TIMEOUT"                       , "/erp/redis/connectionTimeout"}},
    {ConfigurationKey::REDIS_CONNECTIONPOOL_SIZE                      , {"ERP_REDIS_CONNECTIONPOOL_SIZE"                      , "/erp/redis/connectionPoolSize"}},
    {ConfigurationKey::REDIS_SENTINEL_HOSTS                           , {"ERP_REDIS_SENTINEL_HOSTS"                           , "/erp/redis/sentinelHosts"}},
    {ConfigurationKey::REDIS_SENTINEL_MASTER_NAME                     , {"ERP_REDIS_SENTINEL_MASTER_NAME"                     , "/erp/redis/sentinelMasterName"}},
    {ConfigurationKey::REDIS_SENTINEL_SOCKET_TIMEOUT                  , {"ERP_REDIS_SENTINEL_SOCKET_TIMEOUT"                  , "/erp/redis/sentinelSocketTimeout"}},
    {ConfigurationKey::TOKEN_ULIMIT_CALLS                             , {"ERP_TOKEN_ULIMIT_CALLS"                             , "/erp/server/token/ulimitCalls"}},
    {ConfigurationKey::TOKEN_ULIMIT_TIMESPAN_MS                       , {"ERP_TOKEN_ULIMIT_TIMESPAN_MS"                       , "/erp/server/token/ulimitTimespanMS"}},
    {ConfigurationKey::REPORT_LEIPS_KEY_ENABLE                        , {"ERP_REPORT_LEIPS_KEY_ENABLE"                        , "/erp/report/leips/enable"}},
    {ConfigurationKey::REPORT_LEIPS_KEY_REFRESH_INTERVAL_SECONDS      , {"ERP_REPORT_LEIPS_KEY_REFRESH_INTERVAL_SECONDS"      , "/erp/report/leips/refreshIntervalSeconds"}},
    {ConfigurationKey::REPORT_LEIPS_KEY_CHECK_INTERVAL_SECONDS        , {"ERP_REPORT_LEIPS_KEY_CHECK_INTERVAL_SECONDS"        , "/erp/report/leips/checkIntervalSeconds"}},
    {ConfigurationKey::REPORT_LEIPS_FAILED_KEY_CHECK_INTERVAL_SECONDS , {"ERP_REPORT_LEIPS_FAILED_KEY_CHECK_INTERVAL_SECONDS" , "/erp/report/leips/failedCheckIntervalSeconds"}},
    {ConfigurationKey::XML_SCHEMA_MISC                                , {"ERP_XML_SCHEMA_MISC"                                , "/erp/xml-schema", Flags::array}},
    {ConfigurationKey::FHIR_PROFILE_OLD_VALID_UNTIL                   , {"ERP_FHIR_PROFILE_OLD_VALID_UNTIL"                   , "/erp/fhir-profile-old/valid-until"}},
    {ConfigurationKey::FHIR_PROFILE_OLD_XML_SCHEMA_KBV                , {"ERP_FHIR_PROFILE_OLD_XML_SCHEMA_KBV"                , "/erp/fhir-profile-old/kbv.ita.erp", Flags::array}},
    {ConfigurationKey::FHIR_PROFILE_OLD_XML_SCHEMA_GEMATIK            , {"ERP_FHIR_PROFILE_OLD_XML_SCHEMA_GEMATIK"            , "/erp/fhir-profile-old/de.gematik.erezept-workflow.r4", Flags::array}},
    {ConfigurationKey::FHIR_STRUCTURE_DEFINITIONS_OLD                 , {"ERP_FHIR_STRUCTURE_DEFINITIONS_OLD"                 , "/erp/fhir-profile-old/fhir/structure-files", Flags::array}},
    {ConfigurationKey::ERP_FHIR_VERSION_OLD                           , {"ERP_FHIR_VERSION_OLD"                               , "/erp/fhir-profile-old/erp-fhir-version"}},
    {ConfigurationKey::FHIR_PROFILE_OLD_VALIDATION_LEVELS_UNREFERENCED_BUNDLED_RESOURCE, {"ERP_OLD_PROFILE_FHIR_VALIDATION_LEVELS_UNREFERENCED_BUNDLED_RESOURCE", "/erp/fhir-profile-old/fhir-validation/levels/unreferenced-bundled-resource"}},
    {ConfigurationKey::FHIR_PROFILE_OLD_VALIDATION_LEVELS_UNREFERENCED_CONTAINED_RESOURCE, {"ERP_OLD_PROFILE_FHIR_VALIDATION_LEVELS_UNREFERENCED_CONTAINED_RESOURCE", "/erp/fhir-profile-old/fhir-validation/levels/unreferenced-contained-resource"}},
    {ConfigurationKey::FHIR_PROFILE_OLD_VALIDATION_LEVELS_MANDATORY_RESOLVABLE_REFERENCE_FAILURE, {"ERP_OLD_PROFILE_FHIR_VALIDATION_LEVELS_MANDATORY_RESOLVABLE_REFERENCE_FAILURE", "/erp/fhir-profile-old/fhir-validation/levels/mandatory-resolvable-reference-failure"}},
    {ConfigurationKey::FHIR_STRUCTURE_DEFINITIONS                     , {"ERP_FHIR_STRUCTURE_DEFINITIONS"                     , "/erp/fhir-profile/fhir/structure-files", Flags::array}},
    {ConfigurationKey::ERP_FHIR_VERSION                               , {"ERP_FHIR_VERSION"                                   , "/erp/fhir-profile/erp-fhir-version"}},
    {ConfigurationKey::FHIR_PROFILE_VALID_FROM                        , {"ERP_FHIR_PROFILE_VALID_FROM"                        , "/erp/fhir-profile/valid-from"}},
    {ConfigurationKey::FHIR_PROFILE_RENDER_FROM                       , {"ERP_FHIR_PROFILE_RENDER_FROM"                       , "/erp/fhir-profile/render-from"}},
    {ConfigurationKey::FHIR_PROFILE_XML_SCHEMA_KBV                    , {"ERP_FHIR_PROFILE_XML_SCHEMA_KBV"                    , "/erp/fhir-profile/kbv.ita.erp", Flags::array}},
    {ConfigurationKey::FHIR_PROFILE_XML_SCHEMA_GEMATIK                , {"ERP_FHIR_PROFILE_XML_SCHEMA_GEMATIK"                , "/erp/fhir-profile/de.gematik.erezept-workflow.r4", Flags::array}},
    {ConfigurationKey::FHIR_VALIDATION_LEVELS_UNREFERENCED_BUNDLED_RESOURCE, {"ERP_FHIR_VALIDATION_LEVELS_UNREFERENCED_BUNDLED_RESOURCE", "/erp/fhir-profile/fhir-validation/levels/unreferenced-bundled-resource"}},
    {ConfigurationKey::FHIR_VALIDATION_LEVELS_UNREFERENCED_CONTAINED_RESOURCE, {"ERP_FHIR_VALIDATION_LEVELS_UNREFERENCED_CONTAINED_RESOURCE", "/erp/fhir-profile/fhir-validation/levels/unreferenced-contained-resource"}},
    {ConfigurationKey::FHIR_VALIDATION_LEVELS_MANDATORY_RESOLVABLE_REFERENCE_FAILURE, {"ERP_FHIR_VALIDATION_LEVELS_MANDATORY_RESOLVABLE_REFERENCE_FAILURE", "/erp/fhir-profile/fhir-validation/levels/mandatory-resolvable-reference-failure"}},
    {ConfigurationKey::HSM_CACHE_REFRESH_SECONDS                      , {"ERP_HSM_CACHE_REFRESH_SECONDS"                      , "/erp/hsm/cache-refresh-seconds"}},
    {ConfigurationKey::HSM_DEVICE                                     , {"ERP_HSM_DEVICE"                                     , "/erp/hsm/device"}},
    {ConfigurationKey::HSM_MAX_SESSION_COUNT                          , {"ERP_HSM_MAX_SESSION_COUNT"                          , "/erp/hsm/max-session-count"}},
    {ConfigurationKey::HSM_WORK_USERNAME                              , {"ERP_HSM_WORK_USERNAME"                              , "/erp/hsm/work-username"}},
    {ConfigurationKey::HSM_WORK_PASSWORD                              , {"ERP_HSM_WORK_PASSWORD"                              , "/erp/hsm/work-password", Flags::credential}},
    {ConfigurationKey::HSM_WORK_KEYSPEC                               , {"ERP_HSM_WORK_KEYSPEC"                               , "/erp/hsm/work-keyspec"}},
    {ConfigurationKey::HSM_SETUP_USERNAME                             , {"ERP_HSM_SETUP_USERNAME"                             , "/erp/hsm/setup-username"}},
    {ConfigurationKey::HSM_SETUP_PASSWORD                             , {"ERP_HSM_SETUP_PASSWORD"                             , "/erp/hsm/setup-password", Flags::credential}},
    {ConfigurationKey::HSM_SETUP_KEYSPEC                              , {"ERP_HSM_SETUP_KEYSPEC"                              , "/erp/hsm/setup-keyspec"}},
    {ConfigurationKey::HSM_CONNECT_TIMEOUT_SECONDS                    , {"ERP_HSM_CONNECT_TIMEOUT_SECONDS"                    , "/erp/hsm/connect-timeout-seconds"}},
    {ConfigurationKey::HSM_READ_TIMEOUT_SECONDS                       , {"ERP_HSM_READ_TIMEOUT_SECONDS"                       , "/erp/hsm/read-timeout-seconds"}},
    {ConfigurationKey::HSM_IDLE_TIMEOUT_SECONDS                       , {"ERP_HSM_IDLE_TIMEOUT_SECONDS"                       , "/erp/hsm/idle-timeout-seconds"}},
    {ConfigurationKey::HSM_RECONNECT_INTERVAL_SECONDS                 , {"ERP_HSM_RECONNECT_INTERVAL_SECONDS"                 , "/erp/hsm/reconnect-interval-seconds"}},
    {ConfigurationKey::DEPRECATED_HSM_USERNAME                        , {"ERP_HSM_USERNAME"                                   , "/erp/hsm/username"}},
    {ConfigurationKey::DEPRECATED_HSM_PASSWORD                        , {"ERP_HSM_PASWORD"                                    , "/erp/hsm/password", Flags::credential}},
    {ConfigurationKey::TEE_TOKEN_UPDATE_SECONDS                       , {"ERP_TEE_TOKEN_UPDATE_SECONDS"                       , "/erp/hsm/tee-token/update-seconds"}},
    {ConfigurationKey::TEE_TOKEN_RETRY_SECONDS                        , {"ERP_TEE_TOKEN_RETRY_SECONDS"                        , "/erp/hsm/tee-token/retry-seconds"}},
    {ConfigurationKey::TIMING_CATEGORIES                              , {"ERP_TIMING_CATEGORIES"                              , "/erp/timingCategories", Flags::array}},
    {ConfigurationKey::ZSTD_DICTIONARY_DIR                            , {"ERP_ZSTD_DICTIONARY_DIR"                            , "/erp/compression/zstd/dictionary-dir"}},
    {ConfigurationKey::HTTPCLIENT_CONNECT_TIMEOUT_SECONDS             , {"ERP_HTTPCLIENT_CONNECT_TIMEOUT_SECONDS"             , "/erp/httpClientConnectTimeoutSeconds"}},
    {ConfigurationKey::FEATURE_PKV                                    , {"ERP_FEATURE_PKV"                                    , "/erp/feature/pkv"}},
    {ConfigurationKey::ADMIN_SERVER_INTERFACE                         , {"ERP_ADMIN_SERVER_INTERFACE"                         , "/erp/admin/server/interface"}},
    {ConfigurationKey::ADMIN_SERVER_PORT                              , {"ERP_ADMIN_SERVER_PORT"                              , "/erp/admin/server/port"}},
    {ConfigurationKey::ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS           , {"ERP_ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS"           , "/erp/admin/defaultShutdownDelaySeconds"}},
    {ConfigurationKey::ADMIN_CREDENTIALS                              , {"ERP_ADMIN_CREDENTIALS"                              , "/erp/admin/credentials", Flags::credential}},
    {ConfigurationKey::VSDM_PROOF_VALIDITY_SECONDS                    , {"ERP_PROOF_VALIDITY_SECONDS"                         , "/erp/vsdmValiditySeconds"}},
    });
    // clang-format on
}

DevConfigKeyNames::DevConfigKeyNames()
    : OpsConfigKeyNames()
{
    // clang-format off
    mNamesByKey.insert({
    {ConfigurationKey::DEBUG_ENABLE_HSM_MOCK,                 {"DEBUG_ENABLE_HSM_MOCK",             "/debug/enable-hsm-mock"}},
    {ConfigurationKey::DEBUG_DISABLE_REGISTRATION,            {"DEBUG_DISABLE_REGISTRATION",        "/debug/disable-registration"}},
    {ConfigurationKey::DEBUG_DISABLE_DOS_CHECK,               {"DEBUG_DISABLE_DOS_CHECK",           "/debug/disable-dos-check"}},
    {ConfigurationKey::DEBUG_DISABLE_QES_ID_CHECK,            {"DEBUG_DISABLE_QES_ID_CHECK",        "/debug/disable-qes-id-check"}},
    {ConfigurationKey::DEBUG_ENABLE_MOCK_TSL_MANAGER,         {"DEBUG_ENABLE_MOCK_TSL_MANAGER",     "/debug/enable-mock-tsl-manager"}}
    });
    // clang-format on
}

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
        throw ExceptionWrapper<InvalidConfigurationException>::create({__FILE__, __LINE__},
            std::string("Configuration: can not convert '") + value.value() + "' to integer",
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
        throw createMissingKeyException(key, {__FILE__, __LINE__});
}

std::string ConfigurationBase::getStringValue (const KeyNames key) const
{
    const auto value = getStringValueInternal(key);
    if (value.has_value())
        return value.value();
    else
        throw createMissingKeyException(key, {__FILE__, __LINE__});
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
        throw createMissingKeyException(key, {__FILE__, __LINE__});
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
        throw createMissingKeyException(key, {__FILE__, __LINE__});
}


bool ConfigurationBase::getOptionalBoolValue (KeyNames key, const bool defaultValue) const
{
    const auto value = getBoolValueInternal(key);
    if (value.has_value())
        return value.value();
    else
        return defaultValue;
}

namespace
{
template<typename JsonArray>
std::vector<std::string> getArrayHelper(const JsonArray& jsonArray)
{
    std::vector<std::string> result;
    result.reserve(jsonArray.Size());
    for (const auto& val : jsonArray)
    {
        Expect3(val.IsString(), "Configuration values in array must be strings", std::logic_error);
        result.emplace_back(val.GetString());
    }
    return result;
}
}

std::vector<std::string> ConfigurationBase::getArrayInternal(KeyNames key) const
{
    auto arr = getOptionalArrayInternal(key);
    if (arr.empty())
    {
        throw createMissingKeyException(key, {__FILE__, __LINE__});
    }
    return arr;
}

std::vector<std::string> ConfigurationBase::getOptionalArrayInternal(KeyNames key) const
{
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
        return getArrayHelper(jsonValue->GetArray());
    }
    return result;
}

std::map<std::string, std::vector<std::string>> ConfigurationBase::getMapInternal(KeyNames key) const
{
    std::map<std::string, std::vector<std::string>> ret;
    const auto jsonValueIter = mValuesByKey.find(std::string(key.jsonPath));
    if (jsonValueIter != mValuesByKey.end())
    {
        const auto* jsonValue = jsonValueIter->second;
        Expect3(jsonValue != nullptr, "JSON value must not be nullptr", std::logic_error);
        Expect3(jsonValue->IsObject(), "JSON value must be object", std::logic_error);
        const auto& jsonObj = jsonValue->GetObject();
        for (const auto& member : jsonObj)
        {
            Expect3(member.name.IsString(), "JSON map keys must be string", std::logic_error);
            Expect3(member.value.IsArray(), "JSON map values must be array", std::logic_error);
            ret.emplace(member.name.GetString(), getArrayHelper(member.value.GetArray()));
        }
        return ret;
    }
    throw createMissingKeyException(key, {__FILE__, __LINE__});
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

static bool allDefined(const std::optional<model::Timestamp>& oldValidUntil,
                       const std::optional<std::string>& oldErpFhirVersion, const std::vector<std::string>& oldKbvSchemas,
                       const std::vector<std::string>& oldGematikSchemas,
                       const std::optional<model::Timestamp>& profileRenderFrom,
                       const std::optional<model::Timestamp>& profileValidFrom)
{
    return oldValidUntil && profileRenderFrom && oldErpFhirVersion && ! oldKbvSchemas.empty() &&
           ! oldGematikSchemas.empty() && profileValidFrom;
}

static bool noneDefined(const std::optional<model::Timestamp>& oldValidUntil,
                        const std::optional<std::string>& oldErpFhirVersion, const std::vector<std::string>& oldKbvSchemas,
                        const std::vector<std::string>& oldGematikSchemas,
                        const std::optional<model::Timestamp>& profileRenderFrom,
                        const std::optional<model::Timestamp>& profileValidFrom)
{
    const auto now = model::Timestamp::now();
    return (! oldValidUntil || *oldValidUntil <= now) && (! profileRenderFrom || *profileRenderFrom <= now) &&
           ! oldErpFhirVersion && oldKbvSchemas.empty() && oldGematikSchemas.empty() &&
           (! profileValidFrom || *profileValidFrom <= now);
}

static void ensureCorrectOldProfileConfiguration(const std::optional<model::Timestamp>& oldValidUntil,
                                                 const std::optional<std::string>& oldErpFhirVersion,
                                                 const std::vector<std::string>& oldKbvSchemas,
                                                 const std::vector<std::string>& oldGematikSchemas,
                                                 const std::optional<model::Timestamp>& profileRenderFrom,
                                                 const std::optional<model::Timestamp>& profileValidFrom)
{
    Expect(allDefined(oldValidUntil, oldErpFhirVersion, oldKbvSchemas, oldGematikSchemas,
                      profileRenderFrom, profileValidFrom) ||
               noneDefined(oldValidUntil, oldErpFhirVersion, oldKbvSchemas, oldGematikSchemas,
                           profileRenderFrom, profileValidFrom),
           "configuration for next schema incomplete, all values must be defined: FHIR_PROFILE_OLD_VALID_UNTIL, "
           "FHIR_PROFILE_OLD_XML_SCHEMA_KBV_VERSION, FHIR_PROFILE_OLD_XML_SCHEMA_KBV, "
           "FHIR_PROFILE_OLD_XML_SCHEMA_GEMATIK_VERSION, "
           "FHIR_PROFILE_OLD_XML_SCHEMA_GEMATIK, FHIR_PROFILE_VALID_FROM, FHIR_PROFILE_RENDER_FROM");

    if (oldValidUntil && profileValidFrom)
    {
        using namespace std::chrono_literals;
        Expect(*oldValidUntil >= *profileValidFrom, "The two configured profiles have a validity gap in between");
    }
}

void configureXmlValidator(XmlValidator& xmlValidator)
{
    const auto& configuration = Configuration::instance();

    xmlValidator.loadSchemas(configuration.getArray(ConfigurationKey::XML_SCHEMA_MISC));

    // This is the new profile, or the only profile if only one is supported
    const auto& profileValidFromStr =
        configuration.getOptionalStringValue(ConfigurationKey::FHIR_PROFILE_VALID_FROM);
    std::optional<model::Timestamp> profileValidfrom =
        profileValidFromStr ? std::make_optional(model::Timestamp::fromXsDateTime(*profileValidFromStr))
                             : std::nullopt;
    const auto& profileRenderFromStr =
        configuration.getOptionalStringValue(ConfigurationKey::FHIR_PROFILE_RENDER_FROM);
    const auto& profileRenderFrom = profileRenderFromStr
                                     ? std::make_optional(model::Timestamp::fromXsDateTime(*profileRenderFromStr)) : std::nullopt;
    const auto& profileKbvSchemas = configuration.getOptionalArray(ConfigurationKey::FHIR_PROFILE_XML_SCHEMA_KBV);
    const auto& erpFhirVersion = configuration.getStringValue(ConfigurationKey::ERP_FHIR_VERSION);
    const auto& profileGematikSchemas =
        configuration.getOptionalArray(ConfigurationKey::FHIR_PROFILE_XML_SCHEMA_GEMATIK);

    // this is the optional old profile, that is still valid for some grace-period.
    const auto& oldValidUntilStr = configuration.getOptionalStringValue(ConfigurationKey::FHIR_PROFILE_OLD_VALID_UNTIL);
    const auto& oldValidUntil =
        oldValidUntilStr ? std::make_optional(model::Timestamp::fromXsDateTime(*oldValidUntilStr)) : std::nullopt;
    const auto& oldKbvSchemas = configuration.getOptionalArray(ConfigurationKey::FHIR_PROFILE_OLD_XML_SCHEMA_KBV);
    const auto& oldErpFhirVersion = configuration.getOptionalStringValue(ConfigurationKey::ERP_FHIR_VERSION_OLD);
    const auto& oldGematikSchemas =
        configuration.getOptionalArray(ConfigurationKey::FHIR_PROFILE_OLD_XML_SCHEMA_GEMATIK);

    ensureCorrectOldProfileConfiguration(oldValidUntil, oldErpFhirVersion, oldKbvSchemas, oldGematikSchemas,
                                         profileRenderFrom, profileValidfrom);

    // check that the configured versions do actually exist
    (void)model::ResourceVersion::str_vBundled(erpFhirVersion);


    if (oldValidUntilStr && oldErpFhirVersion)
    {
        (void)model::ResourceVersion::str_vBundled(*oldErpFhirVersion);
        xmlValidator.loadSchemas(oldKbvSchemas, std::nullopt, oldValidUntil);
        xmlValidator.loadSchemas(oldGematikSchemas, std::nullopt, oldValidUntil);
    }
    Expect3(profileKbvSchemas.empty(), "FHIR_PROFILE_XML_SCHEMA_KBV must not contain any schemas", std::logic_error);
    Expect3(profileGematikSchemas.empty(),
            "FHIR_PROFILE_XML_SCHEMA_GEMATIK must not contain any schemas", std::logic_error);
}

static std::optional<uint16_t> getPortFromConfiguration (const Configuration& configuration, const ConfigurationKey key)
{
    const auto port = configuration.getOptionalIntValue(key);
    if (port.has_value())
    {
        Expect(port.value() > 0 && port.value() <= 65535, "default port number is out of range");
        return std::optional<uint16_t>(static_cast<uint16_t>(port.value()));
    }
    else
        return {};
}

std::optional<uint16_t> getEnrolementServerPort (
    const uint16_t pcPort,
    const uint16_t defaultEnrolmentServerPort)
{
    const auto enrolmentPort = getPortFromConfiguration(Configuration::instance(), ConfigurationKey::ENROLMENT_SERVER_PORT)
                                   .value_or(defaultEnrolmentServerPort);
    const auto activeForPcPort = Configuration::instance().getOptionalIntValue(
        ConfigurationKey::ENROLMENT_ACTIVATE_FOR_PORT,
        -1);
    if (activeForPcPort != pcPort)
    {
        TLOG(INFO) << "Enrolment-Server (on port=" << enrolmentPort << ") is disabled (ERP_SERVER_PORT = " << pcPort
                     <<") != (ENROLMENT_ACTIVATE_FOR_PORT = "<< activeForPcPort << ")";
    }

    return activeForPcPort == pcPort ? std::optional<uint16_t>(enrolmentPort) : std::nullopt;
}

fhirtools::ValidatorOptions
Configuration::defaultValidatorOptions(model::ResourceVersion::FhirProfileBundleVersion bundleVersion,
                                       SchemaType schemaType) const
{
    using enum ConfigurationKey;
    using Severity = fhirtools::Severity;
    const auto& config = Configuration::instance();
    fhirtools::ValidatorOptions options;
    switch (bundleVersion)
    {
        using enum model::ResourceVersion::FhirProfileBundleVersion;
        case v_2022_01_01:
            options.levels.mandatoryResolvableReferenceFailure =
                config.get<Severity>(FHIR_PROFILE_OLD_VALIDATION_LEVELS_MANDATORY_RESOLVABLE_REFERENCE_FAILURE);
            options.levels.unreferencedBundledResource =
                config.get<Severity>(FHIR_PROFILE_OLD_VALIDATION_LEVELS_UNREFERENCED_BUNDLED_RESOURCE);
            options.levels.unreferencedContainedResource =
                config.get<Severity>(FHIR_PROFILE_OLD_VALIDATION_LEVELS_UNREFERENCED_CONTAINED_RESOURCE);
            break;
        case v_2023_07_01:
            options.levels.mandatoryResolvableReferenceFailure =
                config.get<Severity>(FHIR_VALIDATION_LEVELS_MANDATORY_RESOLVABLE_REFERENCE_FAILURE);
            options.levels.unreferencedBundledResource =
                config.get<Severity>( FHIR_VALIDATION_LEVELS_UNREFERENCED_BUNDLED_RESOURCE);
            options.levels.unreferencedContainedResource =
                config.get<Severity>(FHIR_VALIDATION_LEVELS_UNREFERENCED_CONTAINED_RESOURCE);
            break;
    }
    if (schemaType == SchemaType::KBV_PR_ERP_Bundle)
    {
        options.allowNonLiteralAuthorReference = kbvValidationNonLiteralAuthorRef() == NonLiteralAutherRefMode::allow;
    }
    return options;
}

const Configuration& Configuration::instance()
{
    static const Configuration theInstance;
    return theInstance;
}

void Configuration::check() const
{
    (void) kbvValidationOnUnknownExtension();
    (void) kbvValidationNonLiteralAuthorRef();
    for (auto bundleVer: magic_enum::enum_values<model::ResourceVersion::FhirProfileBundleVersion>())
    {
        (void)genericValidationMode(bundleVer);
        (void)defaultValidatorOptions(bundleVer, SchemaType::fhir);
    }
    (void) getOptional<fhirtools::Severity>(ConfigurationKey::FHIR_VALIDATION_LEVELS_UNREFERENCED_BUNDLED_RESOURCE,
                                            fhirtools::Severity::error);
    (void) getOptional<fhirtools::Severity>(ConfigurationKey::FHIR_VALIDATION_LEVELS_UNREFERENCED_CONTAINED_RESOURCE,
                                            fhirtools::Severity::error);
    (void) getOptional<fhirtools::Severity>(
        ConfigurationKey::FHIR_VALIDATION_LEVELS_MANDATORY_RESOLVABLE_REFERENCE_FAILURE, fhirtools::Severity::error);
    (void) getIntValue(ConfigurationKey::VSDM_PROOF_VALIDITY_SECONDS);
}

bool Configuration::featurePkvEnabled() const
{
    return getOptionalBoolValue(ConfigurationKey::FEATURE_PKV, false);
}

Configuration::OnUnknownExtension Configuration::kbvValidationOnUnknownExtension() const
{
    return getOptional<OnUnknownExtension>(ConfigurationKey::SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION,
                                           OnUnknownExtension::report);
}

Configuration::NonLiteralAutherRefMode Configuration::kbvValidationNonLiteralAuthorRef() const
{
    return getOptional<NonLiteralAutherRefMode>(
        ConfigurationKey::SERVICE_TASK_ACTIVATE_KBV_VALIDATION_NON_LITERAL_AUTHOR_REF, NonLiteralAutherRefMode::allow);
}


Configuration::GenericValidationMode
Configuration::genericValidationMode(model::ResourceVersion::FhirProfileBundleVersion bundleVersion) const
{
    switch (bundleVersion)
    {
        using enum model::ResourceVersion::FhirProfileBundleVersion;
        case v_2022_01_01:
            return getOptional<GenericValidationMode>(ConfigurationKey::SERVICE_OLD_PROFILE_GENERIC_VALIDATION_MODE,
                                                    GenericValidationMode::ignore_errors);
        case v_2023_07_01:
            return GenericValidationMode::require_success;
    }
    Fail2("unknown FhirProfileBundleVersion: " + std::to_string(static_cast<intmax_t>(bundleVersion)), std::logic_error);
}

bool Configuration::timingLoggingEnabled(const std::string& category) const
{
    static const std::unordered_set<std::string> configArray = [this] {
        auto array = getArray(ConfigurationKey::TIMING_CATEGORIES);
        std::unordered_set<std::string> s;
        std::ranges::copy(array, std::inserter(s, s.begin()));
        return s;
    }();
    static const bool allEnabled = configArray.contains("all");
    return allEnabled || configArray.contains(category);
}


template fhirtools::Severity
    ConfigurationTemplate<ConfigurationKey, ConfigurationKeyNames>::getOptional<fhirtools::Severity>(
        ConfigurationKey, std::type_identity_t<fhirtools::Severity>) const;
