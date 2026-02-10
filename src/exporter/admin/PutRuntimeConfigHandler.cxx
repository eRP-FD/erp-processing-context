// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "exporter/admin/PutRuntimeConfigHandler.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "exporter/server/SessionContext.hxx"
#include "exporter/util/RuntimeConfiguration.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/TLog.hxx"
#include "shared/util/UrlHelper.hxx"

namespace exporter
{

namespace
{
std::optional<RuntimeConfiguration::ProcessorType> processorTypeFromParam(const std::string& param)
{
    const auto processorLower = String::toLower(param);
    if (processorLower == RuntimeConfiguration::parameter_epa)
    {
        return RuntimeConfiguration::ProcessorType::EPA;
    }
    if (processorLower == RuntimeConfiguration::parameter_t_rezept)
    {
        return RuntimeConfiguration::ProcessorType::T_REZEPT;
    }
    if (processorLower.empty() || processorLower == "true")
    {
        return std::nullopt;
    }
    ErpFail(HttpStatus::BadRequest, "invalid pause/resume parameter: " + param);
}
}

PutRuntimeConfigHandler::PutRuntimeConfigHandler(ConfigurationKey adminRcCredentialsKey)
    : AdminRequestHandlerBase(adminRcCredentialsKey)
{
}

Operation PutRuntimeConfigHandler::getOperation() const
{
    return Operation::PUT_Admin_exporter_configuration;
}

void PutRuntimeConfigHandler::doHandleRequest(BaseSessionContext& baseSession)
{
    auto& session = dynamic_cast<::exporter::SessionContext&>(baseSession);
    TVLOG(1) << "PUT /admin/configuration: " << session.request.header().serializeFields();
    TVLOG(1) << "PUT /admin/configuration: " << session.request.getBody();

    const auto& contentType = session.request.header().header(Header::ContentType);
    ErpExpect(contentType == MimeType::xWwwFormUrlEncoded, HttpStatus::BadRequest,
              "unsupported content type: " + contentType.value_or("<none>"));

    const auto params = UrlHelper::splitQuery(session.request.getBody());

    auto runtimeConfig = session.serviceContext.getRuntimeConfigurationSetter();

    for (const auto& param : params)
    {
        TVLOG(1) << param.first << ": " << param.second;
        if (param.first == exporter::RuntimeConfiguration::parameter_pause)
        {
            handleParameterPause(*runtimeConfig, param.second);
        }
        else if (param.first == exporter::RuntimeConfiguration::parameter_resume)
        {
            handleParameterResume(*runtimeConfig, param.second);
        }
        else if (param.first == exporter::RuntimeConfiguration::parameter_throttle)
        {
            handleParameterThrottle(*runtimeConfig, param.second);
        }
        else if (RuntimeConfiguration::parameter_metrics_category_log_threshold_ms.contains(param.first))
        {
            handleParameterMetricsLogThreshold(*runtimeConfig, param.first, param.second);
        }
        else
        {
            ErpFail(HttpStatus::BadRequest, "unknown parameter in request body: " + param.first + "=" + param.second);
        }
    }
}

void PutRuntimeConfigHandler::handleParameterPause(RuntimeConfigurationSetter& runtimeConfig, const std::string& param)
{
    // GEMREQ-start A_27859
    if (const auto processor = processorTypeFromParam(param))
    {
        TLOG(INFO) << "admin: pause " << magic_enum::enum_name(*processor);
        runtimeConfig.pause(*processor);
    }
    else
    {
        TLOG(INFO) << "admin: pause all";
        for (const auto processorType : magic_enum::enum_values<RuntimeConfiguration::ProcessorType>())
        {
            runtimeConfig.pause(processorType);
        }
    }
    // GEMREQ-end A_27859
}
void PutRuntimeConfigHandler::handleParameterResume(RuntimeConfigurationSetter& runtimeConfig, const std::string& param)
{
    if (const auto processor = processorTypeFromParam(param))
    {
        TLOG(INFO) << "admin: resume " << magic_enum::enum_name(*processor);
        runtimeConfig.resume(*processor);
    }
    else
    {
        TLOG(INFO) << "admin: resume all";
        for (const auto processorType : magic_enum::enum_values<RuntimeConfiguration::ProcessorType>())
        {
            runtimeConfig.resume(processorType);
        }
    }
}
void PutRuntimeConfigHandler::handleParameterThrottle(RuntimeConfigurationSetter& runtimeConfig,
                                                      const std::string& param)
{
    try
    {
        std::size_t idx = 0;
        const int64_t throttleValue = std::stoll(param, &idx);
        ErpExpect(idx == param.size(), HttpStatus::BadRequest, "illegal throttle value: " + param);
        TLOG(INFO) << "admin: throttle value: " << throttleValue;
        runtimeConfig.throttle(RuntimeConfiguration::ThrottleMode::MANUAL, std::chrono::milliseconds(throttleValue));
    }
    catch (const std::logic_error& re)
    {
        ErpFail(HttpStatus::BadRequest,
                (std::ostringstream{} << "invalid: Throttle=" << param << ". " << re.what()).str());
    }
}
void PutRuntimeConfigHandler::handleParameterMetricsLogThreshold(RuntimeConfigurationSetter& runtimeConfig,
                                                                 const std::string& key, const std::string& param)
{
    const auto category = RuntimeConfiguration::parameter_metrics_category_log_threshold_ms.at(key);
    const auto defaults = RuntimeConfiguration::defaultMetricsLogThresholdsMs();
    const auto value = ! param.empty() ? std::chrono::milliseconds{std::stol(param)} : defaults.at(category);
    runtimeConfig.setMetricsLogThresholdMs(category, value);
}
}// namespace exporter
