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

namespace exporter {

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
            TLOG(INFO) << "admin: pause";
            runtimeConfig->pause();
        }
        else if (param.first == exporter::RuntimeConfiguration::parameter_resume)
        {
            TLOG(INFO) << "admin: resume";
            runtimeConfig->resume();
        }
        else if (param.first == exporter::RuntimeConfiguration::parameter_throttle)
        {
            try
            {
                std::size_t idx = 0;
                const int64_t throttleValue = std::stoll(param.second, &idx);
                ErpExpect(idx == param.second.size(), HttpStatus::BadRequest,
                          "illegal throttle value: " + param.second);
                TLOG(INFO) << "admin: throttle value: " << throttleValue;
                runtimeConfig->throttle(std::chrono::milliseconds(throttleValue));
            }
            catch (const std::logic_error& re)
            {
                ErpFail(HttpStatus::BadRequest,
                        (std::ostringstream{} << "invalid: " << param.first << "=" << param.second << ". " << re.what())
                            .str());
            }
        }
        else
        {
            ErpFail(HttpStatus::BadRequest, "unknown parameter in request body: " + param.first + "=" + param.second);
        }
    }
}

} // namespace exporter
