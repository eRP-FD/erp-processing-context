// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
// non-exclusively licensed to gematik GmbH

#include "erp/admin/PutRuntimeConfigHandler.hxx"

#include <erp/pc/PcServiceContext.hxx>
#include <erp/server/context/SessionContext.hxx>
#include <erp/server/request/ServerRequest.hxx>
#include <erp/util/RuntimeConfiguration.hxx>
#include <erp/util/UrlHelper.hxx>

PutRuntimeConfigHandler::PutRuntimeConfigHandler()
    : AdminRequestHandlerBase(ConfigurationKey::ADMIN_RC_CREDENTIALS)
{
}

Operation PutRuntimeConfigHandler::getOperation() const
{
    return Operation::PUT_Admin_pn3_configuration;
}

void PutRuntimeConfigHandler::doHandleRequest(SessionContext& session)
{
    TVLOG(1) << "PUT /admin/configuration: " << session.request.header().serializeFields();
    TVLOG(1) << "PUT /admin/configuration: " << session.request.getBody();

    const auto& contentType = session.request.header().header(Header::ContentType);
    ErpExpect(contentType == MimeType::xWwwFormUrlEncoded, HttpStatus::BadRequest,
              "unsupported content type: " + contentType.value_or("<none>"));

    const auto params = UrlHelper::splitQuery(session.request.getBody());

    auto runtimeConfig = session.serviceContext.getRuntimeConfigurationSetter();
    bool enableAcceptPn3 = runtimeConfig->isAcceptPN3Enabled();
    model::Timestamp acceptPn3Expiry{model::Timestamp::now() + RuntimeConfiguration::accept_pn3_max_active};

    for (const auto& param : params)
    {
        if (param.first == RuntimeConfiguration::parameter_accept_pn3)
        {
            const auto paramValue = String::toLower(param.second);
            enableAcceptPn3 = paramValue == "true";
        }
        else if (param.first == RuntimeConfiguration::parameter_accept_pn3_expiry)
        {
            try
            {
                acceptPn3Expiry = model::Timestamp::fromXsDateTime(UrlHelper::unescapeUrl(param.second));
            }
            catch (const std::runtime_error& re)
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

    if (enableAcceptPn3)
    {
        runtimeConfig->enableAcceptPN3(acceptPn3Expiry);
    }
    else
    {
        runtimeConfig->disableAcceptPN3();
    }
}
