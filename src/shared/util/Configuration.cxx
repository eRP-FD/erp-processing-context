/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/Configuration.hxx"
#include "shared/ErpConstants.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/GLog.hxx"
#include "shared/util/InvalidConfigurationException.hxx"
#include "shared/util/String.hxx"
#include "shared/validation/XmlValidator.hxx"
#include "fhirtools/repository/FhirResourceGroupConfiguration.hxx"
#include "fhirtools/repository/FhirResourceViewConfiguration.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"

#include <charconv>
#include <regex>
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

const std::map<model::ProfileType, ConfigurationBase::ProfileTypeRequirement> ConfigurationBase::ERP::requiredProfiles{
    {model::ProfileType::ActivateTaskParameters, {}},
    {model::ProfileType::CreateTaskParameters, {}},
    {model::ProfileType::Gem_erxAuditEvent, {}},
    {model::ProfileType::Gem_erxBinary, {}},
    {model::ProfileType::fhir, {}},// general FHIR schema
    {model::ProfileType::Gem_erxCommunicationDispReq, {}},
    {model::ProfileType::Gem_erxCommunicationInfoReq, {}},
    {model::ProfileType::Gem_erxCommunicationChargChangeReq, {}},
    {model::ProfileType::Gem_erxCommunicationChargChangeReply, {}},
    {model::ProfileType::Gem_erxCommunicationReply, {}},
    {model::ProfileType::Gem_erxCommunicationRepresentative, {}},
    {model::ProfileType::Gem_erxCompositionElement, {}},
    {model::ProfileType::Gem_erxDevice, {}},
    {model::ProfileType::Gem_erxDigest, {}},
    {model::ProfileType::GEM_ERP_PR_Medication, {{"GEM_2025-01-15"}}},
    {model::ProfileType::GEM_ERP_PR_PAR_CloseOperation_Input, {{"GEM_2025-01-15"}}},
    {model::ProfileType::GEM_ERP_PR_PAR_DispenseOperation_Input, {{"GEM_2025-01-15"}}},
    {model::ProfileType::KBV_PR_ERP_Bundle, {}},
    {model::ProfileType::KBV_PR_ERP_Composition, {}},
    {model::ProfileType::KBV_PR_ERP_Medication_Compounding, {}},
    {model::ProfileType::KBV_PR_ERP_Medication_FreeText, {}},
    {model::ProfileType::KBV_PR_ERP_Medication_Ingredient, {}},
    {model::ProfileType::KBV_PR_ERP_Medication_PZN, {}},
    {model::ProfileType::KBV_PR_ERP_PracticeSupply, {}},
    {model::ProfileType::KBV_PR_ERP_Prescription, {}},
    {model::ProfileType::KBV_PR_FOR_Coverage, {}},
    {model::ProfileType::KBV_PR_FOR_Organization, {}},
    {model::ProfileType::KBV_PR_FOR_Patient, {}},
    {model::ProfileType::KBV_PR_FOR_Practitioner, {}},
    {model::ProfileType::KBV_PR_FOR_PractitionerRole, {}},
    {model::ProfileType::Gem_erxMedicationDispense, {}},
    {model::ProfileType::MedicationDispenseBundle, {}},
    {model::ProfileType::Gem_erxReceiptBundle, {}},
    {model::ProfileType::Gem_erxTask, {}},
    {model::ProfileType::Gem_erxChargeItem, {}},
    {model::ProfileType::Gem_erxConsent, {}},
    {model::ProfileType::PatchChargeItemParameters, {}},
    {model::ProfileType::DAV_DispenseItem, {}},
    {model::ProfileType::Subscription, {}},
    {model::ProfileType::OperationOutcome, {}},
};

const std::map<model::ProfileType, ConfigurationBase::ProfileTypeRequirement>
    ConfigurationBase::MedicationExporter::requiredProfiles{
        {model::ProfileType::fhir, {}},// general FHIR schema
        {model::ProfileType::OperationOutcome, {}},
        {model::ProfileType::ProvideDispensationErpOp, {}},
        {model::ProfileType::ProvidePrescriptionErpOp, {}},
        {model::ProfileType::CancelPrescriptionErpOp, {}},
        {model::ProfileType::EPAOpRxPrescriptionERPOutputParameters, {}},
        {model::ProfileType::EPAOpRxDispensationERPOutputParameters, {}},
        {model::ProfileType::OrganizationDirectory, {}},
        {model::ProfileType::EPAMedicationPZNIngredient, {}},
    };

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
        const char* filePathVariable, const std::filesystem::path& configLocation)
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

    ExceptionWrapper<InvalidConfigurationException> createMissingKeyException(const KeyData key,
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


ConfigurationBase::ConfigurationBase (const std::vector<KeyData>& allKeyNames)
    : mDocument(parseConfigFile(ErpConstants::ConfigurationFileNameVariable, std::filesystem::current_path()))
    , mServerPort(determineServerPort())
{
    auto serverHost = Environment::get(ServerHostEnvVar.data());
    Expect(serverHost.has_value(),
           std::string("Environment variable \"") + ServerHostEnvVar.data() + "\" must be set.");
    mServerHost = serverHost.value();

    if (! mDocument.IsObject())
        return;

    const auto* pathPrefix = "";

    for (const auto& keyNames : allKeyNames)
    {
        const auto jsonPath = std::string(keyNames.jsonPath);

        lookupKey(pathPrefix, jsonPath);
    }

    lookupKey("", std::string{ERP::fhirResourceGroups});
    lookupKey("", std::string{ERP::fhirResourceViews});
    lookupKey("", std::string{ERP::synthesizeCodesystemPath});
    lookupKey("", std::string{ERP::synthesizeValuesetPath});

    lookupKey("", std::string{MedicationExporter::fhirResourceGroups});
    lookupKey("", std::string{MedicationExporter::fhirResourceViews});
    lookupKey("", std::string{MedicationExporter::synthesizeCodesystemPath});
    lookupKey("", std::string{MedicationExporter::synthesizeValuesetPath});
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
    using Flags = KeyData::ConfigurationKeyFlags;
    // clang-format off
    mNamesByKey.insert({
    {ConfigurationKey::C_FD_SIG_ERP_VALIDATION_INTERVAL               , {"ERP_C_FD_SIG_ERP_VALIDATION_INTERVAL"               , "/erp/c.fd.sig-erp-validation", Flags::categoryEnvironment, "The OCSP validation interval for C.FD.OSIG-eRP signer certificate in seconds"}},
    {ConfigurationKey::ENROLMENT_SERVER_PORT                          , {"ERP_ENROLMENT_SERVER_PORT"                          , "/erp/enrolment/server/port", Flags::categoryEnvironment, "Port number for the enrolment server"}},
    {ConfigurationKey::ENROLMENT_ACTIVATE_FOR_PORT                    , {"ERP_ENROLMENT_ACTIVATE_FOR_PORT"                    , "/erp/enrolment/server/activateForPort", Flags::categoryEnvironment, "Port of the processing-context for which the enrolment server shall be active"}},
    {ConfigurationKey::ENROLMENT_API_CREDENTIALS                      , {"ERP_ENROLMENT_API_CREDENTIALS"                      , "/erp/enrolment/api/credentials", Flags::credential | Flags::categoryEnvironment, "Basic authentication credentials for enrolment server"}},
    {ConfigurationKey::ENTROPY_FETCH_INTERVAL_SECONDS                 , {"ERP_ENTROPY_FETCH_INTERVAL_SECONDS"                 , "/erp/entropy_fetch_interval_seconds", Flags::categoryFunctionalStatic, "Interval for the refresh of the entropy in seconds"}},
    {ConfigurationKey::IDP_REGISTERED_FD_URI                          , {"ERP_IDP_REGISTERED_FD_URI"                          , "/erp/idp/registeredFdUri", Flags::categoryEnvironment, "The value expected in the aud Claim of the Authentication-Token (JWT)"}},
    {ConfigurationKey::IDP_CERTIFICATE_MAX_AGE_HOURS                  , {"ERP_IDP_CERTIFICATE_MAX_AGE_HOURS"                  , "/erp/idp/certificateMaxAgeHours", Flags::categoryFunctional, "The maximum acceptable age of the Idp-Certificate before health status changes to DOWN (hours)"}},
    {ConfigurationKey::IDP_UPDATE_ENDPOINT                            , {"ERP_IDP_UPDATE_ENDPOINT"                            , "/erp/idp/updateEndpoint", Flags::categoryEnvironment, "Download location for IDP-Configuration"}},
    {ConfigurationKey::IDP_UPDATE_ENDPOINT_SSL_ROOT_CA_PATH           , {"ERP_IDP_UPDATE_ENDPOINT_SSL_ROOT_CA_PATH"           , "/erp/idp/sslRootCaPath", Flags::categoryEnvironment, "Root certificate for IDP download for ERP_IDP_UPDATE_ENDPOINT"}},
    {ConfigurationKey::IDP_UPDATE_INTERVAL_MINUTES                    , {"ERP_IDP_UPDATE_INTERVAL_MINUTES"                    , "/erp/idp/updateIntervalMinutes", Flags::categoryFunctionalStatic, "Update interval for IDP Configuration/Certificate, when the IDP health is up"}},
    {ConfigurationKey::IDP_NO_VALID_CERTIFICATE_UPDATE_INTERVAL_SECONDS, {"ERP_IDP_NO_VALID_CERTIFICATE_UPDATE_INTERVAL_SECONDS", "/erp/idp/noValidCertificateUpdateIntervalSeconds", Flags::categoryFunctionalStatic, "Update interval for IDP Certificate, when the IDP health is down"}},
    {ConfigurationKey::OCSP_C_FD_SIG_ERP_GRACE_PERIOD                 , {"ERP_OCSP_C_FD_SIG_ERP_GRACE_PERIOD"                 , "/erp/ocsp/gracePeriodCFdSigErp", Flags::categoryFunctionalStatic, "OCSP grace period in seconds for OCSP-response of C.FD.OSIG-eRP signer certificate"}},
    {ConfigurationKey::OCSP_SMC_B_OSIG_GRACE_PERIOD                   , {"ERP_OCSP_SMC_B_OSIG_GRACE_PERIOD"                   , "/erp/ocsp/gracePeriodSmcBOsig", Flags::categoryFunctionalStatic, "OCSP Grace period in seconds for OCSP-response of SMC-B certificate from CAdES-BES packet provided in ChargeItem-Post/-Put request"}},
    {ConfigurationKey::OCSP_NON_QES_GRACE_PERIOD                      , {"ERP_OCSP_NON_QES_GRACE_PERIOD"                      , "/erp/ocsp/gracePeriodNonQes", Flags::categoryFunctionalStatic, "OCSP Grace period in seconds for OCSP-response of non-QES Certificates. According to A_20158 for IDP Certificate"}},
    {ConfigurationKey::OCSP_QES_GRACE_PERIOD                          , {"ERP_OCSP_QES_GRACE_PERIOD"                          , "/erp/ocsp/gracePeriodQes", Flags::categoryFunctionalStatic, "OCSP-Grace period in seconds for OCSP-response of QES Certificate"}},
    {ConfigurationKey::SERVER_THREAD_COUNT                            , {"ERP_SERVER_THREAD_COUNT"                            , "/erp/server/thread-count", Flags::categoryEnvironment, "Number of threads to process requests from the VAU Proxy"}},
    {ConfigurationKey::SERVER_CERTIFICATE                             , {"ERP_SERVER_CERTIFICATE"                             , "/erp/server/certificate", Flags::categoryEnvironment, "TLS-Server Certificate used for https endpoints"}},
    {ConfigurationKey::SERVER_PRIVATE_KEY                             , {"ERP_SERVER_PRIVATE_KEY"                             , "/erp/server/certificateKey", Flags::categoryEnvironment|Flags::credential, "Private key for ERP_SERVER_CERTIFICATE"}},
    {ConfigurationKey::SERVER_PROXY_CERTIFICATE                       , {"ERP_SERVER_PROXY_CERTIFICATE"                       , "/erp/server/proxy/certificate", Flags::categoryEnvironment, "Certificate to verify Client-Certificate for connections on tee-server"}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_ENTLASSREZEPT_VALIDITY_WD, {"ERP_SERVICE_TASK_ACTIVATE_ENTLASSREZEPT_VALIDITY_WD", "/erp/service/task/activate/entlassRezeptValidityInWorkDays", Flags::categoryFunctional, "Validity duration for Entlassrezepte in days. Including the day of issuing."}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_HOLIDAYS                 , {"ERP_SERVICE_TASK_ACTIVATE_HOLIDAYS"                 , "/erp/service/task/activate/holidays", Flags::categoryFunctionalStatic|Flags::array, "Dates that shall not count as workday, i.e. public holidays. The holidays by name are enum values of holidays, that have no fixed dates."}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_EASTER_CSV               , {"ERP_SERVICE_TASK_ACTIVATE_EASTER_CSV"               , "/erp/service/task/activate/easterCsv", Flags::categoryFunctionalStatic, "Path to a csv file that contains all easter dates. The dates have the format \"mm-dd\""}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION, {"ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION", "/erp/service/task/activate/kbvValidationOnUnknownExtension", Flags::categoryFunctional, "ignore: Do not check for unknown extensions. report: respond with HTTP 202 Accepted instead of 200 OK, when a KBV-Bundle contains unknown extensions. reject: reject with HTTP 400 Bad Request, when a KBV-Bundle contains unknown extensions"}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_KBV_VALIDATION_NON_LITERAL_AUTHOR_REF, {"ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_NON_LITERAL_AUTHOR_REF", "/erp/service/task/activate/kbvValidationNonLiteralAuthorRef", Flags::categoryFunctional, "Controls the Validation of the field Composition.author."}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_ANR_VALIDATION_MODE       ,{"ERP_SERVICE_TASK_ACTIVATE_ANR_VALIDATION_MODE"      , "/erp/service/task/activate/anrChecksumValidationMode", Flags::categoryFunctional, "Mode for validating ANR/ZANR. Allowed values: warning, error."}},
    {ConfigurationKey::SERVICE_TASK_ACTIVATE_MVOID_VALIDATION_MODE     ,{"ERP_SERVICE_TASK_ACTIVATE_MVOID_VALIDATION_MODE"    , "/erp/service/task/activate/mvoIdValidationMode", Flags::categoryFunctional, "Mode for validating MVO-ID. Allowed values: disable, error."}},
    {ConfigurationKey::SERVICE_TASK_CLOSE_DEVICE_REF_TYPE             ,{"ERP_SERVICE_TASK_CLOSE_DEVICE_REF_TYPE" , "/erp/service/task/close/deviceRefType", Flags::categoryFunctional, "Reference type for Prescription-Digest in Receipt-Bundle. relative: Use relative reference; uuid: use UUID reference"}},
    {ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_REF_TYPE,{"ERP_SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_REF_TYPE" , "/erp/service/task/close/prescriptionDigestRefType", Flags::categoryFunctional, "Reference type for Prescription-Digest in Receipt-Bundle. relative: Use relative reference; uuid: use UUID reference"}},
    {ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_VERSION_ID,{"ERP_SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_VERSION_ID", "/erp/service/task/close/prescriptionDigestMetaVersionId", Flags::categoryFunctional, "Value for Meta.versionId in Prescription-Digest Binary-Resource. If not provided, the field will not be included."}},
    {ConfigurationKey::SERVICE_COMMUNICATION_MAX_MESSAGES             , {"ERP_SERVICE_COMMUNICATION_MAX_MESSAGES"             , "/erp/service/communication/maxMessageCount", Flags::categoryFunctional, "Maximum number of communication messages per task and representative"}},
    {ConfigurationKey::SERVICE_SUBSCRIPTION_SIGNING_KEY               , {"ERP_SERVICE_SUBSCRIPTION_SIGNING_KEY"               , "/erp/service/subscription/signingKey", Flags::credential|Flags::categoryEnvironment, "Key to sign the header and payload of an incoming subscription"}},
    {ConfigurationKey::PCR_SET                                        , {"ERP_PCR_SET"                                        , "/erp/service/pcr-set", Flags::categoryEnvironment, "PCR register set to be used to calculate quote (TPM)"}},
    {ConfigurationKey::POSTGRES_HOST                                  , {"ERP_POSTGRES_HOST"                                  , "/erp/postgres/host", Flags::categoryEnvironment, "Postgres server host"}},
    {ConfigurationKey::POSTGRES_PORT                                  , {"ERP_POSTGRES_PORT"                                  , "/erp/postgres/port", Flags::categoryEnvironment, "Postgres server port number"}},
    {ConfigurationKey::POSTGRES_USER                                  , {"ERP_POSTGRES_USER"                                  , "/erp/postgres/user", Flags::categoryEnvironment, "Postgres user name"}},
    {ConfigurationKey::POSTGRES_PASSWORD                              , {"ERP_POSTGRES_PASSWORD"                              , "/erp/postgres/password", Flags::credential | Flags::categoryEnvironment, "Postgres user password"}},
    {ConfigurationKey::POSTGRES_DATABASE                              , {"ERP_POSTGRES_DATABASE"                              , "/erp/postgres/database", Flags::categoryEnvironment, "Postgres database name"}},
    {ConfigurationKey::POSTGRES_SSL_ROOT_CERTIFICATE_PATH             , {"ERP_POSTGRES_CERTIFICATE_PATH"                      , "/erp/postgres/certificatePath", Flags::categoryEnvironment, "This parameter specifies the name of a file containing SSL certificate authority (CA) certificate(s)"}},
    {ConfigurationKey::POSTGRES_SSL_CERTIFICATE_PATH                  , {"ERP_POSTGRES_SSL_CERTIFICATE_PATH"                  , "/erp/postgres/sslCertificatePath", Flags::categoryEnvironment, "This parameter specifies the file name of the client SSL certificate"}},
    {ConfigurationKey::POSTGRES_SSL_KEY_PATH                          , {"ERP_POSTGRES_SSL_KEY_PATH"                          , "/erp/postgres/sslKeyPath", Flags::categoryEnvironment, "This parameter specifies the location for the secret key used for the client certificate"}},
    {ConfigurationKey::POSTGRES_USESSL                                , {"ERP_POSTGRES_USESSL"                                , "/erp/postgres/useSsl", Flags::categoryEnvironment, "Sets Postgres connection parameter sslmode to require if true. sslmode remains on its default value otherwise"}},
    {ConfigurationKey::POSTGRES_CONNECT_TIMEOUT_SECONDS               , {"ERP_POSTGRES_CONNECT_TIMEOUT_SECONDS"               , "/erp/postgres/connectTimeoutSeconds", Flags::categoryEnvironment, "Maximum time to wait while connecting, in seconds (write as a decimal integer, e.g., 10). Zero, negative, or not specified means wait indefinitely. The minimum allowed timeout is 2 seconds, therefore a value of 1 is interpreted as 2. This timeout applies separately to each host name or IP address. For example, if you specify two hosts and connect_timeout is 5, each host will time out if no connection is made within 5 seconds, so the total time spent waiting for a connection might be up to 10 seconds"}},
    {ConfigurationKey::POSTGRES_ENABLE_SCRAM_AUTHENTICATION           , {"ERP_POSTGRES_ENABLE_SCRAM_AUTHENTICATION"           , "/erp/postgres/enableScramAuthentication", Flags::categoryEnvironment, "Sets Postgres connection parameter channel_binding to 'require'. channel_binding remains on its default value otherwise"}},
    {ConfigurationKey::POSTGRES_TCP_USER_TIMEOUT_MS                   , {"ERP_POSTGRES_TCP_USER_TIMEOUT_MS"                   , "/erp/postgres/tcpUserTimeoutMs", Flags::categoryEnvironment, "Controls the number of milliseconds that transmitted data may remain unacknowledged before a connection is forcibly closed. A value of zero uses the system default. This parameter is ignored for connections made via a Unix-domain socket. It is only supported on systems where TCP_USER_TIMEOUT is available; on other systems, it has no effect."}},
    {ConfigurationKey::POSTGRES_KEEPALIVES_IDLE_SEC                   , {"ERP_POSTGRES_KEEPALIVES_IDLE_SEC"                   , "/erp/postgres/keepalivesIdleSec", Flags::categoryEnvironment, "Controls the number of seconds of inactivity after which TCP should send a keepalive message to the server. A value of zero uses the system default."}},
    {ConfigurationKey::POSTGRES_KEEPALIVES_INTERVAL_SEC               , {"ERP_POSTGRES_KEEPALIVES_INTERVAL_SEC"               , "/erp/postgres/keepalivesIntervalSec", Flags::categoryEnvironment, "Controls the number of seconds after which a TCP keepalive message that is not acknowledged by the server should be retransmitted. A value of zero uses the system default."}},
    {ConfigurationKey::POSTGRES_KEEPALIVES_COUNT                      , {"ERP_POSTGRES_KEEPALIVES_COUNT"                      , "/erp/postgres/keepalivesCount", Flags::categoryEnvironment, "Controls the number of TCP keepalives that can be lost before the client's connection to the server is considered dead. A value of zero uses the system default"}},
    {ConfigurationKey::POSTGRES_TARGET_SESSION_ATTRS                  , {"ERP_POSTGRES_TARGET_SESSION_ATTRS"                  , "/erp/postgres/targetSessionAttrs", Flags::categoryEnvironment, "If this parameter is set to read-write, only a connection in which read-write transactions are accepted by default is considered acceptable. The query SHOW transaction_read_only will be sent upon any successful connection; if it returns on, the connection will be closed. If multiple hosts were specified in the connection string, any remaining servers will be tried just as if the connection attempt had failed. The default value of this parameter, any, regards all connections as acceptable."}},
    {ConfigurationKey::POSTGRES_CONNECTION_MAX_AGE_MINUTES            , {"ERP_POSTGRES_CONNECTION_MAX_AGE_MINUTES"            , "/erp/postgres/connectionMaxAgeMinutes", Flags::categoryEnvironment, "After this time the database connections will be closed and re-opened."}},
    {ConfigurationKey::PUBLIC_E_PRESCRIPTION_SERVICE_URL              , {"ERP_E_PRESCRIPTION_SERVICE_URL"                     , "/erp/publicEPrescriptionServiceUrl", Flags::categoryEnvironment, "Used as basis for links in outgoing resources, e.g. fullUrl"}},
    {ConfigurationKey::REGISTRATION_HEARTBEAT_INTERVAL_SEC            , {"ERP_REGISTRATION_HEARTBEAT_INTERVAL_SEC"            , "/erp/registration/heartbeatIntervalSec", Flags::categoryEnvironment, "interval for the regular health check and registration status update."}},
    {ConfigurationKey::TSL_TI_OCSP_PROXY_URL                          , {"ERP_TSL_TI_OCSP_PROXY_URL"                          , "/erp/tsl/tiOcspProxyUrl", Flags::categoryEnvironment, "Special handling for G0 QES certificates for which no mapping exists in the TSL. In this case a special TI OCSP proxy should be used."}},
    {ConfigurationKey::TSL_FRAMEWORK_SSL_ROOT_CA_PATH                 , {"ERP_TSL_FRAMEWORK_SSL_ROOT_CA_PATH"                 , "/erp/tsl/sslRootCaPath", Flags::categoryEnvironment, "Path to the root CA to be used for the https-TSL-Update URL connection."}},
    {ConfigurationKey::TSL_INITIAL_DOWNLOAD_URL                       , {"ERP_TSL_INITIAL_DOWNLOAD_URL"                       , "/erp/tsl/initialDownloadUrl", Flags::categoryEnvironment, "The URL to download initial TSL from."}},
    {ConfigurationKey::TSL_INITIAL_CA_DER_PATH                        , {"ERP_TSL_INITIAL_CA_DER_PATH"                        , "/erp/tsl/initialCaDerPath", Flags::categoryEnvironment, "Path to the TSL-Signer CA."}},
    {ConfigurationKey::TSL_INITIAL_CA_DER_PATH_NEW                    , {"ERP_TSL_INITIAL_CA_DER_PATH_NEW"                    , "/erp/tsl/initialCaDerPathNew", Flags::categoryEnvironment, "Path to the additional TSL-Signer CA. It could be used when TSL-Signer CA is being changed to support both old and new TSL-Signer CA."}},
    {ConfigurationKey::TSL_INITIAL_CA_DER_PATH_NEW_START              , {"ERP_TSL_INITIAL_CA_DER_PATH_NEW_START"              , "/erp/tsl/initialCaDerPathStart", Flags::categoryEnvironment, "The timestamp in FHIR DateTime format https://www.hl7.org/fhir/datatypes.html#dateTime to use the additional TSL-Signer CA from. Using this variable the additional TSL-Signer CA can be configured before it is active."}},
    {ConfigurationKey::TSL_REFRESH_INTERVAL                           , {"ERP_TSL_REFRESH_INTERVAL"                           , "/erp/tsl/refreshInterval", Flags::categoryFunctional, "How often the TSL update should be tried."}},
    {ConfigurationKey::TSL_DOWNLOAD_CIPHERS                           , {"ERP_TSL_DOWNLOAD_CIPHERS"                           , "/erp/tsl/downloadCiphers", Flags::categoryFunctionalStatic, "Specifies ciphers to be used for TSL download if set."}},
    {ConfigurationKey::JSON_META_SCHEMA                               , {"ERP_JSON_META_SCHEMA"                               , "/erp/json-meta-schema", Flags::categoryFunctionalStatic, "Path to JSON meta-schema for json schema validation"}},
    {ConfigurationKey::JSON_SCHEMA                                    , {"ERP_JSON_SCHEMA"                                    , "/erp/json-schema", Flags::categoryFunctionalStatic|Flags::array, "List of JSON schemas"}},
    {ConfigurationKey::REDIS_DATABASE                                 , {"ERP_REDIS_DATABASE"                                 , "/erp/redis/database", Flags::categoryEnvironment, "Redis database name"}},
    {ConfigurationKey::REDIS_USER                                     , {"ERP_REDIS_USERNAME"                                 , "/erp/redis/user", Flags::categoryEnvironment, "Redis user name"}},
    {ConfigurationKey::REDIS_PASSWORD                                 , {"ERP_REDIS_PASSWORD"                                 , "/erp/redis/password", Flags::categoryEnvironment|Flags::credential, "Redis user password"}},
    {ConfigurationKey::REDIS_HOST                                     , {"ERP_REDIS_HOST"                                     , "/erp/redis/host", Flags::categoryEnvironment, "Redis host name"}},
    {ConfigurationKey::REDIS_PORT                                     , {"ERP_REDIS_PORT"                                     , "/erp/redis/port", Flags::categoryEnvironment, "Redis port"}},
    {ConfigurationKey::REDIS_CERTIFICATE_PATH                         , {"ERP_REDIS_CERTIFICATE_PATH"                         , "/erp/redis/certificatePath", Flags::categoryEnvironment, "Path to trusted SSL certificate authority certificate(s)"}},
    {ConfigurationKey::REDIS_CONNECTION_TIMEOUT                       , {"ERP_REDIS_CONNECTION_TIMEOUT"                       , "/erp/redis/connectionTimeout", Flags::categoryEnvironment, "Connection timeout in ms"}},
    {ConfigurationKey::REDIS_DOS_SOCKET_TIMEOUT                       , {"ERP_REDIS_DOS_SOCKET_TIMEOUT"                       , "/erp/redis/dosSocketTimeoutMs", Flags::categoryEnvironment, "Socket timeout used for DOS checks in ms"}},
    {ConfigurationKey::REDIS_CONNECTIONPOOL_SIZE                      , {"ERP_REDIS_CONNECTIONPOOL_SIZE"                      , "/erp/redis/connectionPoolSize", Flags::categoryEnvironment, "Number of parallel redis connections"}},
    {ConfigurationKey::REDIS_SENTINEL_HOSTS                           , {"ERP_REDIS_SENTINEL_HOSTS"                           , "/erp/redis/sentinelHosts", Flags::categoryEnvironment, "Use redis in sentinel mode and use the given sentinel hosts, separated by comma"}},
    {ConfigurationKey::REDIS_SENTINEL_MASTER_NAME                     , {"ERP_REDIS_SENTINEL_MASTER_NAME"                     , "/erp/redis/sentinelMasterName", Flags::categoryEnvironment, "Redis sentinel master name"}},
    {ConfigurationKey::REDIS_SENTINEL_SOCKET_TIMEOUT                  , {"ERP_REDIS_SENTINEL_SOCKET_TIMEOUT"                  , "/erp/redis/sentinelSocketTimeout", Flags::categoryEnvironment, "Timeout for sending or receiving from sentinel host"}},
    {ConfigurationKey::TOKEN_ULIMIT_CALLS                             , {"ERP_TOKEN_ULIMIT_CALLS"                             , "/erp/server/token/ulimitCalls", Flags::categoryEnvironment, "Specify how many requests per time slot are allowed until it is interpreted as a DoS attack."}},
    {ConfigurationKey::TOKEN_ULIMIT_TIMESPAN_MS                       , {"ERP_TOKEN_ULIMIT_TIMESPAN_MS"                       , "/erp/server/token/ulimitTimespanMS", Flags::categoryEnvironment, "Time slot for counting incoming requests until it is interpreted as a DoS attack."}},
    {ConfigurationKey::REPORT_LEIPS_KEY_ENABLE                        , {"ERP_REPORT_LEIPS_KEY_ENABLE"                        , "/erp/report/leips/enable", Flags::categoryFunctional, "Enable hashing of telematik id for report generation"}},
    {ConfigurationKey::REPORT_LEIPS_KEY_REFRESH_INTERVAL_SECONDS      , {"ERP_REPORT_LEIPS_KEY_REFRESH_INTERVAL_SECONDS"      , "/erp/report/leips/refreshIntervalSeconds", Flags::categoryFunctionalStatic, "Maximum age of the pseudoname_key in seconds from the time of its creation until it is considered as expired."}},
    {ConfigurationKey::REPORT_LEIPS_KEY_CHECK_INTERVAL_SECONDS        , {"ERP_REPORT_LEIPS_KEY_CHECK_INTERVAL_SECONDS"        , "/erp/report/leips/checkIntervalSeconds", Flags::categoryFunctionalStatic, "Interval in seconds to check for pseudoname_key expiration."}},
    {ConfigurationKey::REPORT_LEIPS_FAILED_KEY_CHECK_INTERVAL_SECONDS , {"ERP_REPORT_LEIPS_FAILED_KEY_CHECK_INTERVAL_SECONDS" , "/erp/report/leips/failedCheckIntervalSeconds", Flags::categoryFunctionalStatic, "Retry-Interval in seconds to check for pseudoname_key expiration, when the last call failed"}},
    {ConfigurationKey::XML_SCHEMA_MISC                                , {"ERP_XML_SCHEMA_MISC"                                , "/erp/xml-schema", Flags::array|Flags::categoryFunctionalStatic, "File names of additional XML schemas"}},
    {ConfigurationKey::FHIR_STRUCTURE_DEFINITIONS                     , {"ERP_FHIR_STRUCTURE_DEFINITIONS"                     , "/erp/fhir/structure-files", Flags::categoryFunctionalStatic|Flags::array, "Fhir structure files for generic validation of new profiles"}},
    {ConfigurationKey::FHIR_VALIDATION_LEVELS_UNREFERENCED_BUNDLED_RESOURCE, {"ERP_FHIR_VALIDATION_LEVELS_UNREFERENCED_BUNDLED_RESOURCE", "/erp/fhir/validation/levels/unreferenced-bundled-resource", Flags::categoryFunctionalStatic, "Set severity level for unreferenced entries in bundles of type document in new profiles. Allowed values: debug, info, warning, error"}},
    {ConfigurationKey::FHIR_VALIDATION_LEVELS_UNREFERENCED_CONTAINED_RESOURCE, {"ERP_FHIR_VALIDATION_LEVELS_UNREFERENCED_CONTAINED_RESOURCE", "/erp/fhir/validation/levels/unreferenced-contained-resource", Flags::categoryFunctionalStatic, "Set severity level for unreferenced contained resources in new profiles. Allowed values: debug, info, warning, error"}},
    {ConfigurationKey::FHIR_VALIDATION_LEVELS_MANDATORY_RESOLVABLE_REFERENCE_FAILURE, {"ERP_FHIR_VALIDATION_LEVELS_MANDATORY_RESOLVABLE_REFERENCE_FAILURE", "/erp/fhir/validation/levels/mandatory-resolvable-reference-failure", Flags::categoryFunctionalStatic, "Set severity level for unresolvable references in bundles of type document, that must be resolvable in new profiles. Allowed values: debug, info, warning, error"}},
    {ConfigurationKey::HSM_CACHE_REFRESH_SECONDS                      , {"ERP_HSM_CACHE_REFRESH_SECONDS"                      , "/erp/hsm/cache-refresh-seconds", Flags::categoryEnvironment, "Blob and VSDM++ Key Cache refresh time. Blobs and VSDM Keys are re-read from database in the given interval."}},
    {ConfigurationKey::HSM_DEVICE                                     , {"ERP_HSM_DEVICE"                                     , "/erp/hsm/device", Flags::categoryEnvironment, "Comma separated list of the HSM devices. Format: \"<port>@<IP address>,<port>@<IP address>,...\""}},
    {ConfigurationKey::HSM_MAX_SESSION_COUNT                          , {"ERP_HSM_MAX_SESSION_COUNT"                          , "/erp/hsm/max-session-count", Flags::categoryEnvironment, "Maximum number of parallel connections to HSM"}},
    {ConfigurationKey::HSM_WORK_USERNAME                              , {"ERP_HSM_WORK_USERNAME"                              , "/erp/hsm/work-username", Flags::categoryEnvironment, "Username used to authenticate against HSM (see also ERP_HSM_USERNAME)"}},
    {ConfigurationKey::HSM_WORK_PASSWORD                              , {"ERP_HSM_WORK_PASSWORD"                              , "/erp/hsm/work-password", Flags::credential|Flags::categoryEnvironment, "Password to authenticate ERP_HSM_WORK_USERNAME user against the HSM. ERP_HSM_WORK_KEYSPEC takes precedence over the ERP_HSM_WORK_PASSWORD."}},
    {ConfigurationKey::HSM_WORK_KEYSPEC                               , {"ERP_HSM_WORK_KEYSPEC"                               , "/erp/hsm/work-keyspec", Flags::categoryEnvironment, "Path to the KeySpec File to  authenticate ERP_HSM_WORK_USERNAME user against the HSM. ERP_HSM_WORK_KEYSPEC takes precedence over the ERP_HSM_WORK_PASSWORD."}},
    {ConfigurationKey::HSM_SETUP_USERNAME                             , {"ERP_HSM_SETUP_USERNAME"                             , "/erp/hsm/setup-username", Flags::categoryEnvironment, "Currently unused"}},
    {ConfigurationKey::HSM_SETUP_PASSWORD                             , {"ERP_HSM_SETUP_PASSWORD"                             , "/erp/hsm/setup-password", Flags::categoryEnvironment|Flags::credential, "Currently unused"}},
    {ConfigurationKey::HSM_SETUP_KEYSPEC                              , {"ERP_HSM_SETUP_KEYSPEC"                              , "/erp/hsm/setup-keyspec", Flags::categoryEnvironment, "Currently unused"}},
    {ConfigurationKey::HSM_CONNECT_TIMEOUT_SECONDS                    , {"ERP_HSM_CONNECT_TIMEOUT_SECONDS"                    , "/erp/hsm/connect-timeout-seconds", Flags::categoryEnvironment, "Connection timeout to HSM in seconds."}},
    {ConfigurationKey::HSM_READ_TIMEOUT_SECONDS                       , {"ERP_HSM_READ_TIMEOUT_SECONDS"                       , "/erp/hsm/read-timeout-seconds", Flags::categoryEnvironment, "Timeout of read operations with HSM in seconds."}},
    {ConfigurationKey::HSM_IDLE_TIMEOUT_SECONDS                       , {"ERP_HSM_IDLE_TIMEOUT_SECONDS"                       , "/erp/hsm/idle-timeout-seconds", Flags::categoryEnvironment, "Interval used to perform regular keep-alive calls to the HSM (calling getRandomData(1))"}},
    {ConfigurationKey::HSM_RECONNECT_INTERVAL_SECONDS                 , {"ERP_HSM_RECONNECT_INTERVAL_SECONDS"                 , "/erp/hsm/reconnect-interval-seconds", Flags::categoryEnvironment, "Interval after a failover before retrying a connection to the original HSM in seconds"}},
    {ConfigurationKey::DEPRECATED_HSM_USERNAME                        , {"ERP_HSM_USERNAME"                                   , "/erp/hsm/username", Flags::categoryEnvironment|Flags::deprecated, "Used instead of ERP_HSM_WORK_USERNAME, if the latter is not available"}},
    {ConfigurationKey::DEPRECATED_HSM_PASSWORD                        , {"ERP_HSM_PASWORD"                                    , "/erp/hsm/password", Flags::categoryEnvironment|Flags::credential|Flags::deprecated, "Password used to authenticate against HSM, if neither ERP_HSM_WORK_KEYSPEC nor ERP_HSM_WORK_PASSWORD are set."}},
    {ConfigurationKey::TEE_TOKEN_UPDATE_SECONDS                       , {"ERP_TEE_TOKEN_UPDATE_SECONDS"                       , "/erp/hsm/tee-token/update-seconds", Flags::categoryFunctionalStatic, "Time between regular tee-token updates"}},
    {ConfigurationKey::TEE_TOKEN_RETRY_SECONDS                        , {"ERP_TEE_TOKEN_RETRY_SECONDS"                        , "/erp/hsm/tee-token/retry-seconds", Flags::categoryFunctionalStatic, "Time between tee-token update retries after a failure"}},
    {ConfigurationKey::TIMING_CATEGORIES                              , {"ERP_TIMING_CATEGORIES"                              , "/erp/timingCategories", Flags::categoryEnvironment|Flags::array, "Array of timing categories that shall be logged to INFO level from the following list: redis, postgres, http-client, fhir-validation, ocsp-request, hsm, enrolment, all"}},
    {ConfigurationKey::ZSTD_DICTIONARY_DIR                            , {"ERP_ZSTD_DICTIONARY_DIR"                            , "/erp/compression/zstd/dictionary-dir", Flags::categoryFunctionalStatic, "Path to the compression dictionary for database compression."}},
    {ConfigurationKey::HTTPCLIENT_CONNECT_TIMEOUT_SECONDS             , {"ERP_HTTPCLIENT_CONNECT_TIMEOUT_SECONDS"             , "/erp/httpClientConnectTimeoutSeconds", Flags::categoryEnvironment, "Connection timeout for outgoing tcp connections"}},
    {ConfigurationKey::HTTPCLIENT_RESOLVE_TIMEOUT_MILLISECONDS        , {"ERP_HTTPCLIENT_RESOLVE_TIMEOUT_MILLISECONDS"        , "/erp/httpClientResolveTimeoutMilliseconds", Flags::categoryEnvironment, "Timeout of DNS resolve requests in ms"}},
    {ConfigurationKey::ADMIN_SERVER_INTERFACE                         , {"ERP_ADMIN_SERVER_INTERFACE"                         , "/erp/admin/server/interface", Flags::categoryEnvironment, "The network interface for the admin server binds to"}},
    {ConfigurationKey::ADMIN_SERVER_PORT                              , {"ERP_ADMIN_SERVER_PORT"                              , "/erp/admin/server/port", Flags::categoryEnvironment, "The port for the admin server."}},
    {ConfigurationKey::ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS           , {"ERP_ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS"           , "/erp/admin/defaultShutdownDelaySeconds", Flags::categoryEnvironment, "Default delay for shutdown commands, if no delay is given in request parameter"}},
    {ConfigurationKey::ADMIN_CREDENTIALS                              , {"ERP_ADMIN_CREDENTIALS"                              , "/erp/admin/credentials", Flags::credential|Flags::categoryEnvironment, "HTTP Basic Authentication credential for admin server."}},
    {ConfigurationKey::ADMIN_RC_CREDENTIALS                           , {"ERP_ADMIN_RC_CREDENTIALS"                           , "/erp/admin/runtime_config_credentials", Flags::credential|Flags::categoryEnvironment, "HTTP Basic Authentication credential for runtime configuration through admin server."}},
    {ConfigurationKey::VSDM_PROOF_VALIDITY_SECONDS                    , {"ERP_PROOF_VALIDITY_SECONDS"                         , "/erp/vsdmValiditySeconds", Flags::categoryFunctional, "Validity period for VSDM++ proofs, in seconds"}},

    {ConfigurationKey::MEDICATION_EXPORTER_IS_PRODUCTION                                  , {"ERP_MEDICATION_EXPORTER_IS_PRODUCTION"                                  , "/erp-medication-exporter/is-production", Flags::categoryEnvironment, "Set to true if medication-exporter instance is for production environment; false otherwise"}},

    {ConfigurationKey::MEDICATION_EXPORTER_SERVER_THREAD_COUNT                            , {"ERP_MEDICATION_EXPORTER_SERVER_THREAD_COUNT"                            , "/erp-medication-exporter/server/thread-count", Flags::categoryEnvironment, "Number of (parallel) event processor threads"}},
    {ConfigurationKey::MEDICATION_EXPORTER_SERVER_IO_THREAD_COUNT                         , {"ERP_MEDICATION_EXPORTER_SERVER_IO_THREAD_COUNT"                         , "/erp-medication-exporter/server/io-thread-count", Flags::categoryEnvironment, "Number of processor threads to perform io"}},

    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_HOST                                  , {"ERP_MEDICATION_EXPORTER_POSTGRES_HOST"                                  , "/erp-medication-exporter/postgres/host", Flags::categoryEnvironment, "Postgres server host"}},
    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_PORT                                  , {"ERP_MEDICATION_EXPORTER_POSTGRES_PORT"                                  , "/erp-medication-exporter/postgres/port", Flags::categoryEnvironment, "Postgres server port number"}},
    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_USER                                  , {"ERP_MEDICATION_EXPORTER_POSTGRES_USER"                                  , "/erp-medication-exporter/postgres/user", Flags::categoryEnvironment, "Postgres user name"}},
    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_PASSWORD                              , {"ERP_MEDICATION_EXPORTER_POSTGRES_PASSWORD"                              , "/erp-medication-exporter/postgres/password", Flags::credential | Flags::categoryEnvironment, "Postgres user password"}},
    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_DATABASE                              , {"ERP_MEDICATION_EXPORTER_POSTGRES_DATABASE"                              , "/erp-medication-exporter/postgres/database", Flags::categoryEnvironment, "Postgres database name"}},
    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_SSL_ROOT_CERTIFICATE_PATH             , {"ERP_MEDICATION_EXPORTER_POSTGRES_CERTIFICATE_PATH"                      , "/erp-medication-exporter/postgres/certificatePath", Flags::categoryEnvironment, "This parameter specifies the name of a file containing SSL certificate authority (CA) certificate(s)"}},
    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_SSL_CERTIFICATE_PATH                  , {"ERP_MEDICATION_EXPORTER_POSTGRES_SSL_CERTIFICATE_PATH"                  , "/erp-medication-exporter/postgres/sslCertificatePath", Flags::categoryEnvironment, "This parameter specifies the file name of the client SSL certificate"}},
    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_SSL_KEY_PATH                          , {"ERP_MEDICATION_EXPORTER_POSTGRES_SSL_KEY_PATH"                          , "/erp-medication-exporter/postgres/sslKeyPath", Flags::categoryEnvironment, "This parameter specifies the location for the secret key used for the client certificate"}},
    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_USESSL                                , {"ERP_MEDICATION_EXPORTER_POSTGRES_USESSL"                                , "/erp-medication-exporter/postgres/useSsl", Flags::categoryEnvironment, "Sets Postgres connection parameter sslmode to require if true. sslmode remains on its default value otherwise"}},
    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_CONNECT_TIMEOUT_SECONDS               , {"ERP_MEDICATION_EXPORTER_POSTGRES_CONNECT_TIMEOUT_SECONDS"               , "/erp-medication-exporter/postgres/connectTimeoutSeconds", Flags::categoryEnvironment, "Maximum time to wait while connecting, in seconds (write as a decimal integer, e.g., 10). Zero, negative, or not specified means wait indefinitely. The minimum allowed timeout is 2 seconds, therefore a value of 1 is interpreted as 2. This timeout applies separately to each host name or IP address. For example, if you specify two hosts and connect_timeout is 5, each host will time out if no connection is made within 5 seconds, so the total time spent waiting for a connection might be up to 10 seconds"}},
    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_ENABLE_SCRAM_AUTHENTICATION           , {"ERP_MEDICATION_EXPORTER_POSTGRES_ENABLE_SCRAM_AUTHENTICATION"           , "/erp-medication-exporter/postgres/enableScramAuthentication", Flags::categoryEnvironment, "Sets Postgres connection parameter channel_binding to 'require'. channel_binding remains on its default value otherwise"}},
    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_TCP_USER_TIMEOUT_MS                   , {"ERP_MEDICATION_EXPORTER_POSTGRES_TCP_USER_TIMEOUT_MS"                   , "/erp-medication-exporter/postgres/tcpUserTimeoutMs", Flags::categoryEnvironment, "Controls the number of milliseconds that transmitted data may remain unacknowledged before a connection is forcibly closed. A value of zero uses the system default. This parameter is ignored for connections made via a Unix-domain socket. It is only supported on systems where TCP_USER_TIMEOUT is available; on other systems, it has no effect."}},
    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_KEEPALIVES_IDLE_SEC                   , {"ERP_MEDICATION_EXPORTER_POSTGRES_KEEPALIVES_IDLE_SEC"                   , "/erp-medication-exporter/postgres/keepalivesIdleSec", Flags::categoryEnvironment, "Controls the number of seconds of inactivity after which TCP should send a keepalive message to the server. A value of zero uses the system default."}},
    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_KEEPALIVES_INTERVAL_SEC               , {"ERP_MEDICATION_EXPORTER_POSTGRES_KEEPALIVES_INTERVAL_SEC"               , "/erp-medication-exporter/postgres/keepalivesIntervalSec", Flags::categoryEnvironment, "Controls the number of seconds after which a TCP keepalive message that is not acknowledged by the server should be retransmitted. A value of zero uses the system default."}},
    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_KEEPALIVES_COUNT                      , {"ERP_MEDICATION_EXPORTER_POSTGRES_KEEPALIVES_COUNT"                      , "/erp-medication-exporter/postgres/keepalivesCount", Flags::categoryEnvironment, "Controls the number of TCP keepalives that can be lost before the client's connection to the server is considered dead. A value of zero uses the system default"}},
    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_TARGET_SESSION_ATTRS                  , {"ERP_MEDICATION_EXPORTER_POSTGRES_TARGET_SESSION_ATTRS"                  , "/erp-medication-exporter/postgres/targetSessionAttrs", Flags::categoryEnvironment, "If this parameter is set to read-write, only a connection in which read-write transactions are accepted by default is considered acceptable. The query SHOW transaction_read_only will be sent upon any successful connection; if it returns on, the connection will be closed. If multiple hosts were specified in the connection string, any remaining servers will be tried just as if the connection attempt had failed. The default value of this parameter, any, regards all connections as acceptable."}},
    {ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_CONNECTION_MAX_AGE_MINUTES            , {"ERP_MEDICATION_EXPORTER_POSTGRES_CONNECTION_MAX_AGE_MINUTES"            , "/erp-medication-exporter/postgres/connectionMaxAgeMinutes", Flags::categoryEnvironment, "After this time the database connections will be closed and re-opened."}},

    {ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_CONNECT_TIMEOUT_SECONDS      , {"ERP_MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_CONNECT_TIMEOUT_SECONDS"      , "/erp-medication-exporter/epa-account-lookup/connectTimeoutSeconds", Flags::categoryEnvironment, "HTTPs Client configuration"}},
    {ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_RESOLVE_TIMEOUT_MILLISECONDS , {"ERP_MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_RESOLVE_TIMEOUT_MILLISECONDS" , "/erp-medication-exporter/epa-account-lookup/resolveTimeoutMilliseconds", Flags::categoryEnvironment, "HTTPs Client configuration"}},
    {ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_ENFORCE_SERVER_AUTHENTICATION, {"ERP_MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_ENFORCE_SERVER_AUTHENTICATION", "/erp-medication-exporter/epa-account-lookup/enforceServerAuthentication", Flags::categoryEnvironment, "HTTPs Client configuration"}},
    {ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_ENDPOINT                     , {"ERP_MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_ENDPOINT"                     , "/erp-medication-exporter/epa-account-lookup/endpoint", Flags::categoryEnvironment, "Lookup endpoint on the EPA side"}},
    {ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_USER_AGENT                   , {"ERP_MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_USER_AGENT"                   , "/erp-medication-exporter/epa-account-lookup/userAgent", Flags::categoryEnvironment, "Our user agent used in lookup request"}},
    {ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_ERP_SUBMISSION_FUNCTION_ID   , {"ERP_MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_ERP_SUBMISSION_FUNCTION_ID"   , "/erp-medication-exporter/epa-account-lookup/erpSubmissionFunctionId", Flags::categoryEnvironment, "The name of the function ID for our consent decision"}},
    {ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_EPA_AS_FQDN                  , {"ERP_MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_EPA_AS_FQDN"                  , "/erp-medication-exporter/epa-account-lookup/epaAsFqdn", Flags::categoryEnvironment|Flags::array, "List of EPA FQDN adresses and TEE connection count (default 8) in the format <host>`:`<port>[`+`<connections>] separated by semicolon"}},
    {ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_POOL_SIZE_PER_FQDN           , {"ERP_MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_POOL_SIZE_PER_FQDN"           , "/erp-medication-exporter/epa-account-lookup/poolSizePerFqdn", Flags::categoryEnvironment, "Number of connections per ePA FQDN"}},
    {ConfigurationKey::MEDICATION_EXPORTER_EPA_CONFLICT_WAIT_MINUTES                       , {"MEDICATION_EXPORTER_EPA_CONFLICT_WAIT_MINUTES"                           , "/erp-medication-exporter/epa-conflict/epaConflictWaitMinutes", Flags::categoryEnvironment, "Minutes to wait before retry after EPA conflicts HTTP 409"}},
    {ConfigurationKey::MEDICATION_EXPORTER_RETRIES_MAXIMUM_BEFORE_DEADLETTER               , {"MEDICATION_EXPORTER_RETRIES_MAXIMUM_BEFORE_DEADLETTER"                   , "/erp-medication-exporter/retries/maximumRetriesBeforeDeadletter", Flags::categoryEnvironment, "Maximum retry attempts before moving task events to Deadlettter queue"}},
    {ConfigurationKey::MEDICATION_EXPORTER_RETRIES_RESCHEDULE_DELAY_SECONDS                , {"MEDICATION_EXPORTER_RETRIES_RESCHEDULE_DELAY_SECONDS"                    , "/erp-medication-exporter/retries/rescheduleDelaySeconds", Flags::categoryEnvironment, "Seconds to delay next processing of KVNR after moving events to Deadletter queue"}},

    {ConfigurationKey::MEDICATION_EXPORTER_FHIR_STRUCTURE_DEFINITIONS                      , {"ERP_MEDICATION_EXPORTER_FHIR_STRUCTURE_DEFINITIONS"                      , "/erp-medication-exporter/fhir/structure-files", Flags::categoryEnvironment|Flags::array, "Array of EPA FQDN adresses"}},
    {ConfigurationKey::MEDICATION_EXPORTER_FHIR_STRUCTURE_DEFINITIONS                      , {"ERP_MEDICATION_EXPORTER_FHIR_STRUCTURE_DEFINITIONS"                      , "/erp-medication-exporter/fhir/structure-files", Flags::categoryFunctionalStatic|Flags::array, "Fhir structure files for generic validation and transformation in medication-exporter"}},

    {ConfigurationKey::MEDICATION_EXPORTER_ADMIN_SERVER_INTERFACE                          , {"ERP_MEDICATION_EXPORTER_ADMIN_SERVER_INTERFACE"                          , "/erp-medication-exporter/admin/server/interface", Flags::categoryEnvironment, "The network interface for the admin server binds to"}},
    {ConfigurationKey::MEDICATION_EXPORTER_ADMIN_SERVER_PORT                               , {"ERP_MEDICATION_EXPORTER_ADMIN_SERVER_PORT"                               , "/erp-medication-exporter/admin/server/port", Flags::categoryEnvironment, "The port for the admin server."}},
    {ConfigurationKey::MEDICATION_EXPORTER_ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS            , {"ERP_MEDICATION_EXPORTER_ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS"            , "/erp-medication-exporter/admin/defaultShutdownDelaySeconds", Flags::categoryEnvironment, "Default delay for shutdown commands, if no delay is given in request parameter"}},
    {ConfigurationKey::MEDICATION_EXPORTER_ADMIN_CREDENTIALS                               , {"ERP_MEDICATION_EXPORTER_ADMIN_CREDENTIALS"                               , "/erp-medication-exporter/admin/credentials", Flags::credential|Flags::categoryEnvironment, "HTTP Basic Authentication credential for admin server."}},
    {ConfigurationKey::MEDICATION_EXPORTER_ADMIN_RC_CREDENTIALS                            , {"ERP_MEDICATION_EXPORTER_ADMIN_RC_CREDENTIALS"                            , "/erp-medication-exporter/admin/runtime_config_credentials", Flags::credential|Flags::categoryEnvironment, "HTTP Basic Authentication credential for runtime configuration through admin server."}},


    {ConfigurationKey::MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_CONNECT_TIMEOUT_MILLISECONDS                    , {"MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_CONNECT_TIMEOUT_MILLISECONDS",       "/erp-medication-exporter/vau-https-client/connectTimeoutMilliseconds", Flags::categoryEnvironment, "Timeout in ms during connection attempt."}},
    {ConfigurationKey::MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_SSL_HANDSHAKE_TIMEOUT_MILLISECONDS              , {"MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_SSL_HANDSHAKE_TIMEOUT_MILLISECONDS", "/erp-medication-exporter/vau-https-client/sslHandshakeTimeoutMilliseconds", Flags::categoryEnvironment, "Timeout in ms during ssl handshake"}},
    {ConfigurationKey::MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_MESSAGE_SEND_TIMEOUT_MILLISECONDS               , {"MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_MESSAGE_SEND_TIMEOUT_MILLISECONDS",  "/erp-medication-exporter/vau-https-client/messageSendTimeoutMilliseconds", Flags::categoryEnvironment, "Timeout in ms for sending,"}},
    {ConfigurationKey::MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_MESSAGE_RECEIVE_MILLISECONDS                    , {"MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_MESSAGE_RECEIVE_MILLISECONDS",       "/erp-medication-exporter/vau-https-client/messageReceiveMilliseconds", Flags::categoryEnvironment, "Timeout in ms for receiving."}},
    {ConfigurationKey::MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_RETRY_TIMEOUT_MILLISECONDS                      , {"MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_RETRY_TIMEOUT_MILLISECONDS",         "/erp-medication-exporter/vau-https-client/retryTimeoutMilliseconds", Flags::categoryEnvironment, "Timeout in ms between connection retries."}},
    {ConfigurationKey::MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_RETRIES_PER_ADDRESS                             , {"MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_RETRIES_PER_ADDRESS",                "/erp-medication-exporter/vau-https-client/retriesPerAddress", Flags::categoryEnvironment, "Maximum connect attempts per address"}},
    {ConfigurationKey::MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_RESOLVE_TIMEOUT_MILLISECONDS                    , {"MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_RESOLVE_TIMEOUT_MILLISECONDS",       "/erp-medication-exporter/vau-https-client/resolveTimeoutMilliseconds", Flags::categoryEnvironment, "Timeout for resolving DNS entries."}},
    {ConfigurationKey::MEDICATION_EXPORTER_OCSP_EPA_GRACE_PERIOD                                            , {"MEDICATION_EXPORTER_OCSP_EPA_GRACE_PERIOD"                              , "/erp-medication-exporter/ocsp/gracePeriodEpa", Flags::categoryFunctionalStatic, "OCSP-Grace period in seconds for OCSP-response of ePA Servier Certificate"}},
    // */
    });
    // clang-format on
}

DevConfigKeyNames::DevConfigKeyNames()
    : OpsConfigKeyNames()
{
    using Flags = KeyData::ConfigurationKeyFlags;
    // clang-format off
    mNamesByKey.insert({
    {ConfigurationKey::DEBUG_ENABLE_HSM_MOCK,                 {"DEBUG_ENABLE_HSM_MOCK",             "/debug/enable-hsm-mock", Flags::categoryDebug, "Use HSM mock"}},
    {ConfigurationKey::DEBUG_DISABLE_REGISTRATION,            {"DEBUG_DISABLE_REGISTRATION",        "/debug/disable-registration", Flags::categoryDebug, "Disable VAU registration"}},
    {ConfigurationKey::DEBUG_DISABLE_DOS_CHECK,               {"DEBUG_DISABLE_DOS_CHECK",           "/debug/disable-dos-check", Flags::categoryDebug, "Disable DOS check"}},
    {ConfigurationKey::DEBUG_DISABLE_QES_ID_CHECK,            {"DEBUG_DISABLE_QES_ID_CHECK",        "/debug/disable-qes-id-check", Flags::categoryDebug, "In activateTask, disable validation of "}},
    {ConfigurationKey::DEBUG_ENABLE_MOCK_TSL_MANAGER,         {"DEBUG_ENABLE_MOCK_TSL_MANAGER",     "/debug/enable-mock-tsl-manager", Flags::categoryDebug, "Enable TSL mock"}}
    });
    // clang-format on
}

// -----------------------------------------------------------------------------------------------------

std::optional<int> ConfigurationBase::getIntValueInternal(KeyData key) const
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

std::optional<bool> ConfigurationBase::getBoolValueInternal(KeyData key) const
{
    const auto value = getStringValueInternal(key);
    if (value.has_value())
        return String::toBool(value.value());
    else
        return {};
}

int ConfigurationBase::getOptionalIntValue (const KeyData key, const int defaultValue) const
{
    return getIntValueInternal(key).value_or(defaultValue);
}


int ConfigurationBase::getIntValue (const KeyData key) const
{
    const auto value = getIntValueInternal(key);
    if (value.has_value())
        return value.value();
    else
        throw createMissingKeyException(key, {__FILE__, __LINE__});
}

std::string ConfigurationBase::getStringValue (const KeyData key) const
{
    const auto value = getStringValueInternal(key);
    if (value.has_value())
        return value.value();
    else
        throw createMissingKeyException(key, {__FILE__, __LINE__});
}


std::string ConfigurationBase::getOptionalStringValue (const KeyData key, const std::string& defaultValue) const
{
    const auto value = getStringValueInternal(key);
    if (value.has_value())
        return value.value();
    else
        return defaultValue;
}


std::optional<std::string> ConfigurationBase::getOptionalStringValue (const KeyData key) const
{
    const auto value = getStringValueInternal(key);
    if (value.has_value())
        return value.value();
    else
        return std::nullopt;
}


SafeString ConfigurationBase::getSafeStringValue (const KeyData key) const
{
    auto value = getSafeStringValueInternal(key);
    if (value.has_value())
        return std::move(value.value());
    else
        throw createMissingKeyException(key, {__FILE__, __LINE__});
}


SafeString ConfigurationBase::getOptionalSafeStringValue (const KeyData key, SafeString defaultValue) const
{
    auto value = getSafeStringValueInternal(key);
    if (value.has_value())
        return std::move(value.value());
    else
        return defaultValue;
}


std::optional<SafeString> ConfigurationBase::getOptionalSafeStringValue (const KeyData key) const
{
    return getSafeStringValueInternal(key);
}

std::filesystem::path ConfigurationBase::getPathValue(const KeyData key) const
{
    namespace fs = std::filesystem;
    fs::path path(getStringValue(key));
    return path.lexically_normal();
}

bool ConfigurationBase::getBoolValue (const KeyData key) const
{
    const auto value = getBoolValueInternal(key);
    if (value.has_value())
        return value.value();
    else
        throw createMissingKeyException(key, {__FILE__, __LINE__});
}


bool ConfigurationBase::getOptionalBoolValue (KeyData key, const bool defaultValue) const
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

std::vector<std::string> ConfigurationBase::getArrayInternal(KeyData key) const
{
    const auto& value = Environment::get(std::string{key.environmentVariable});
    if (value)
    {
        return String::split(value.value(), ';');
    }

    const auto* jsonValue = getJsonValue(key);
    if (jsonValue != nullptr)
    {
        Expect3(jsonValue->IsArray(), "JSON value must be array", std::logic_error);
        return getArrayHelper(jsonValue->GetArray());
    }
    throw createMissingKeyException(key, {__FILE__, __LINE__});
}

std::vector<std::string> ConfigurationBase::getOptionalArrayInternal(KeyData key) const
{
    const auto& value = Environment::get(std::string{key.environmentVariable});
    if (value)
    {
        return String::split(value.value(), ';');
    }

    const auto* jsonValue = getJsonValue(key);
    if (jsonValue != nullptr)
    {
        Expect3(jsonValue->IsArray(), "JSON value must be array", std::logic_error);
        return getArrayHelper(jsonValue->GetArray());
    }
    return {};
}

std::map<std::string, std::vector<std::string>> ConfigurationBase::getMapInternal(KeyData key) const
{
    std::map<std::string, std::vector<std::string>> ret;
    const auto* jsonValue = getJsonValue(key);
    if (jsonValue != nullptr)
    {
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

std::optional<std::string> ConfigurationBase::getOptionalStringFromJson(KeyData key) const
{
    const auto* jsonValue = getJsonValue(key);
    if (! jsonValue || jsonValue->IsNull())
    {
        return std::nullopt;
    }
    Expect3(jsonValue->IsString(), "JSON Value must be string", std::logic_error);
    return jsonValue->GetString();
}

std::vector<std::string> ConfigurationBase::getOptionalArrayFromJson(KeyData key) const
{
    const auto* jsonValue = getJsonValue(key);
    if (! jsonValue)
    {
        return {};
    }
    Expect3(jsonValue->IsArray(), "JSON value must be array", std::logic_error);
    return getArrayHelper(jsonValue->GetArray());
}

std::optional<std::string> ConfigurationBase::getStringValueInternal (KeyData key) const
{
    const auto& value = Environment::get(key.environmentVariable.data());
    if (value)
        return value;

    // Could not find the value in the environment. Now try the configuration file.
    return getOptionalStringFromJson(key);
}

const rapidjson::Value* ConfigurationBase::getJsonValue(KeyData key) const
{
    const auto jsonValueIter = mValuesByKey.find(std::string(key.jsonPath));
    if(jsonValueIter != mValuesByKey.end() && jsonValueIter->second != nullptr)
    {
        return jsonValueIter->second;
    }
    return nullptr;
}

std::list<std::pair<std::string, fhirtools::FhirVersion>>
ConfigurationBase::resourceList(const std::string& jsonPath) const
{
    std::list<std::pair<std::string, fhirtools::FhirVersion>> result;
    const auto* resourceArray =
        getJsonValue(KeyData{.environmentVariable = "", .jsonPath = jsonPath, .flags = 0, .description = ""});
    Expect3(resourceArray != nullptr, "missing configuration value: " + jsonPath, std::logic_error);
    Expect3(resourceArray->IsArray(), "configuration value must be Array: " + jsonPath, std::logic_error);
    for (const auto& entry : resourceArray->GetArray())
    {
        Expect3(entry.IsObject(), "configuration value must be Array of Objects: " + jsonPath, std::logic_error);
        const auto& url = entry.FindMember("url");
        Expect3(url != entry.MemberEnd(), "missing url in: " + jsonPath, std::logic_error);
        Expect3(url->value.IsString(), "url must be String: " + jsonPath, std::logic_error);
        const auto& version = entry.FindMember("version");
        Expect3(version != entry.MemberEnd(), "missing version in: " + jsonPath, std::logic_error);
        if (version->value.IsString())
        {
            result.emplace_back(url->value.GetString(), version->value.GetString());
        }
        else if (version->value.IsNull())
        {
            result.emplace_back(url->value.GetString(), fhirtools::FhirVersion::notVersioned);
        }
        else
        {
            Fail2("version must be String or null: " + jsonPath, std::logic_error);
        }
    }
    return result;
}


std::optional<SafeString> ConfigurationBase::getSafeStringValueInternal (const KeyData key) const
{
    const char* value = Environment::getCharacterPointer(key.environmentVariable.data());
    if (value != nullptr)
        return SafeString(value);

    // Could not find the value in the environment. Now try the configuration file.
    const auto* jsonValue = getJsonValue(key);
    if (jsonValue == nullptr || jsonValue->IsNull())
    {
        // Not found in the configuration file either.
        return std::nullopt;
    }
    Expect3(jsonValue->IsString(), "JSON value must be string", std::logic_error);
    return SafeString(jsonValue->GetString());
}

void configureXmlValidator(XmlValidator& xmlValidator)
{
    const auto& configuration = Configuration::instance();
    xmlValidator.loadSchemas(configuration.getArray(ConfigurationKey::XML_SCHEMA_MISC));
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

fhirtools::ValidatorOptions Configuration::defaultValidatorOptions(model::ProfileType profileType) const
{
    using enum ConfigurationKey;
    using Severity = fhirtools::Severity;
    const auto& config = Configuration::instance();
    fhirtools::ValidatorOptions options;
    options.levels.mandatoryResolvableReferenceFailure =
        config.get<Severity>(FHIR_VALIDATION_LEVELS_MANDATORY_RESOLVABLE_REFERENCE_FAILURE);
    options.levels.unreferencedBundledResource =
        config.get<Severity>(FHIR_VALIDATION_LEVELS_UNREFERENCED_BUNDLED_RESOURCE);
    options.levels.unreferencedContainedResource =
        config.get<Severity>(FHIR_VALIDATION_LEVELS_UNREFERENCED_CONTAINED_RESOURCE);
    if (profileType == model::ProfileType::KBV_PR_ERP_Bundle)
    {
        options.allowNonLiteralAuthorReference = kbvValidationNonLiteralAuthorRef() == NonLiteralAuthorRefMode::allow;
        switch (config.kbvValidationOnUnknownExtension())
        {
            using enum Configuration::OnUnknownExtension;
            using ReportUnknownExtensionsMode = fhirtools::ValidatorOptions::ReportUnknownExtensionsMode;
            case ignore:
                options.reportUnknownExtensions = ReportUnknownExtensionsMode::disable;
                break;
            case report:
            case reject:
                // unknown extensions in closed slicing will be reported as error
                // open slices will be unslicedWarning
                options.reportUnknownExtensions = ReportUnknownExtensionsMode::onlyOpenSlicing;
                break;
        }
        return options;
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
    (void) anrChecksumValidationMode();
    (void) defaultValidatorOptions(model::ProfileType::fhir);
    (void) getOptional<fhirtools::Severity>(ConfigurationKey::FHIR_VALIDATION_LEVELS_UNREFERENCED_BUNDLED_RESOURCE,
                                            fhirtools::Severity::error);
    (void) getOptional<fhirtools::Severity>(ConfigurationKey::FHIR_VALIDATION_LEVELS_UNREFERENCED_CONTAINED_RESOURCE,
                                            fhirtools::Severity::error);
    (void) getOptional<fhirtools::Severity>(
        ConfigurationKey::FHIR_VALIDATION_LEVELS_MANDATORY_RESOLVABLE_REFERENCE_FAILURE, fhirtools::Severity::error);
    (void) getIntValue(ConfigurationKey::VSDM_PROOF_VALIDITY_SECONDS);

    (void) prescriptionDigestRefType();
    (void) closeTaskDeviceRefType();
    (void) synthesizeCodesystem<ConfigurationBase::ERP>();
    (void) synthesizeValuesets<ConfigurationBase::ERP>();

    (void) getBoolValue(ConfigurationKey::MEDICATION_EXPORTER_IS_PRODUCTION);
    (void) synthesizeCodesystem<ConfigurationBase::MedicationExporter>();
    (void) synthesizeValuesets<ConfigurationBase::MedicationExporter>();
    for (const auto& epa : this->epaFQDNs())
    {
        LOG(INFO) << "Configured Epa: " << epa.hostName << ':' << epa.port << " with " << epa.teeConnectionCount
                  << " connections";
    }
}

Configuration::OnUnknownExtension Configuration::kbvValidationOnUnknownExtension() const
{
    return getOptional<OnUnknownExtension>(ConfigurationKey::SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION,
                                           OnUnknownExtension::report);
}

Configuration::NonLiteralAuthorRefMode Configuration::kbvValidationNonLiteralAuthorRef() const
{
    return getOptional<NonLiteralAuthorRefMode>(
        ConfigurationKey::SERVICE_TASK_ACTIVATE_KBV_VALIDATION_NON_LITERAL_AUTHOR_REF, NonLiteralAuthorRefMode::allow);
}

Configuration::AnrChecksumValidationMode Configuration::anrChecksumValidationMode() const
{
    return get<AnrChecksumValidationMode>(ConfigurationKey::SERVICE_TASK_ACTIVATE_ANR_VALIDATION_MODE);
}

Configuration::PrescriptionDigestRefType Configuration::prescriptionDigestRefType() const
{
    return get<PrescriptionDigestRefType>(ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_REF_TYPE);
}

Configuration::DeviceRefType Configuration::closeTaskDeviceRefType() const
{
    return get<DeviceRefType>(ConfigurationKey::SERVICE_TASK_CLOSE_DEVICE_REF_TYPE);
}

Configuration::MvoIdValidationMode Configuration::mvoIdValidationMode() const
{
    return get<MvoIdValidationMode>(ConfigurationKey::SERVICE_TASK_ACTIVATE_MVOID_VALIDATION_MODE);
}

template <config::ProcessType ProcessT>
fhirtools::FhirResourceGroupConfiguration Configuration::fhirResourceGroupConfiguration() const
{
    static const auto* groups =
    getJsonValue(KeyData{.environmentVariable = "", .jsonPath = ProcessT::fhirResourceGroups, .flags = 0, .description = ""});
    return fhirtools::FhirResourceGroupConfiguration(groups);
}

template <config::ProcessType ProcessT>
fhirtools::FhirResourceViewConfiguration Configuration::fhirResourceViewConfiguration() const
{
    static const auto* views =
    getJsonValue(KeyData{.environmentVariable = "", .jsonPath = ProcessT::fhirResourceViews, .flags = 0, .description = ""});

    return fhirtools::FhirResourceViewConfiguration{fhirResourceGroupConfiguration<ProcessT>(), views};
}

template <config::ProcessType ProcessT>
std::list<std::pair<std::string, fhirtools::FhirVersion>> Configuration::synthesizeCodesystem() const
{
    return resourceList(std::string{ProcessT::synthesizeCodesystemPath});
}

template <config::ProcessType ProcessT>
std::list<std::pair<std::string, fhirtools::FhirVersion>> Configuration::synthesizeValuesets() const
{
    return resourceList(std::string{ProcessT::synthesizeValuesetPath});
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

std::vector<Configuration::EpaFQDN> Configuration::epaFQDNs() const
{
    static constexpr char formatHelp[] = " - format: <host>`:`<port>[`+`<tee connections>]";
    static constexpr char pattern[] = R"(^(.+):(\d+)(\+(\d+))?$)";
    static constexpr size_t hostIdx = 1;
    static constexpr size_t portIdx = 2;
    // index 3 is connections part including `+` required to make connections optional
    static constexpr size_t connectionsIdx = 4;
    static const std::regex regex{pattern};
    const auto& key = names_.strings(ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_EPA_AS_FQDN);
    const std::string envVarName{key.environmentVariable};
    const auto convert = [&](const auto& match, auto& out, const char* fieldName)
    {
        std::string_view asView{match.first, match.second};
        auto res = std::from_chars(asView.begin(), asView.end(), out);
        Expect(res.ec == std::error_code{},
               ("entry in " + envVarName + ": " + std::make_error_code(res.ec).message() + " for " + fieldName + ": ")
                       .append(asView) +
                   formatHelp);
        Expect(res.ptr == asView.end(),
               ("entry in " + envVarName + ": extra characters for " + fieldName + ": ").append(asView) + formatHelp);
    };
    std::vector<Configuration::EpaFQDN> result;
    auto entries = getArray(ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_EPA_AS_FQDN);
    result.reserve(entries.size());
    for (const auto& entry: entries)
    {
        std::smatch m;
        if (std::regex_match(entry, m, regex))
        {
            Expect3(m.size() == 5, ("matching entry in " + envVarName).append(" failed: ").append(entry), std::logic_error);
            Expect(m[hostIdx].matched && m[portIdx].matched,
                    ("entry in " + envVarName).append(" must at least have hostname and port: ").append(entry).append(formatHelp));
            auto& r = result.emplace_back(m[hostIdx].str());
            convert(m[portIdx], r.port, "<port>");
            if (m[connectionsIdx].matched)
            {
                convert(m[connectionsIdx], r.teeConnectionCount, "<tee connections>");
            }
            Expect(r.port != 0, ("<port> is zero in entry of " + envVarName).append(": ").append(entry));
            Expect(r.teeConnectionCount != 0, ("<tee connections> is zero in entry of " + envVarName).append(": ").append(entry));
        }
        else
        {
            Fail(("invalid format in entry of " + envVarName).append(": ").append(entry).append(formatHelp));
        }
    }
    Expect(!result.empty(), envVarName + " doesn't contain any valid entries");
    return result;
}

template fhirtools::Severity
    ConfigurationTemplate<ConfigurationKey, ConfigurationKeyNames>::getOptional<fhirtools::Severity>(
        ConfigurationKey, std::type_identity_t<fhirtools::Severity>) const;

template fhirtools::FhirResourceGroupConfiguration Configuration::fhirResourceGroupConfiguration<ConfigurationBase::ERP>() const;
template fhirtools::FhirResourceViewConfiguration Configuration::fhirResourceViewConfiguration<ConfigurationBase::ERP>() const;
template std::list<std::pair<std::string, fhirtools::FhirVersion>> Configuration::synthesizeCodesystem<ConfigurationBase::ERP>() const;
template std::list<std::pair<std::string, fhirtools::FhirVersion>> Configuration::synthesizeValuesets<ConfigurationBase::ERP>() const;

template fhirtools::FhirResourceGroupConfiguration Configuration::fhirResourceGroupConfiguration<ConfigurationBase::MedicationExporter>() const;
template fhirtools::FhirResourceViewConfiguration Configuration::fhirResourceViewConfiguration<ConfigurationBase::MedicationExporter>() const;
template std::list<std::pair<std::string, fhirtools::FhirVersion>> Configuration::synthesizeCodesystem<ConfigurationBase::MedicationExporter>() const;
template std::list<std::pair<std::string, fhirtools::FhirVersion>> Configuration::synthesizeValuesets<ConfigurationBase::MedicationExporter>() const;
