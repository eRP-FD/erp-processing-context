/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/admin/AdminRequestHandler.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/TerminationHandler.hxx"
#include "erp/util/UrlHelper.hxx"

void AdminRequestHandlerBase::handleRequest(SessionContext& session)
{
    try
    {
        handleBasicAuthentication(session, ConfigurationKey::ADMIN_CREDENTIALS);
        doHandleRequest(session);
        session.response.setStatus(HttpStatus::OK);
    }
    catch (const ErpException& ex)
    {
        session.response.setStatus(ex.status());
        session.response.setBody(ex.what());
    }
    catch (const std::exception& ex)
    {
        TLOG(ERROR) << "caught std::exception while handling admin request: " << ex.what();
        session.response.setStatus(HttpStatus::InternalServerError);
        session.response.setBody(ex.what());
    }
    catch (...)
    {
        TLOG(ERROR) << "caught unknown exception while handling admin request";
        session.response.setStatus(HttpStatus::InternalServerError);
        session.response.setBody("unknown exception");
    }
}

PostRestartHandler::PostRestartHandler()
    : mDefaultShutdownDelay(
          Configuration::instance().getIntValue(ConfigurationKey::ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS))
{
}

PostRestartHandler::~PostRestartHandler() = default;

void PostRestartHandler::doHandleRequest(SessionContext& session)
{
    TVLOG(1) << "shutdown requested by: " << session.request.header().serializeFields() << "\r\n\r\n"
             << session.request.getBody();
    const auto& contentType = session.request.header().header(Header::ContentType);
    ErpExpect(! contentType.has_value() || contentType == MimeType::xWwwFormUrlEncoded, HttpStatus::BadRequest,
              "unsupported content type: " + contentType.value_or(""));

    auto delay = mDefaultShutdownDelay;
    if (contentType && contentType == ContentMimeType::xWwwFormUrlEncoded)
    {
        const auto params = UrlHelper::splitQuery(session.request.getBody());
        for (const auto& param : params)
        {
            if (param.first == delay_parameter_name)
            {
                size_t idx = 0;
                delay = std::stoi(param.second, &idx);
                ErpExpect(idx == param.second.length(), HttpStatus::BadRequest,
                          "could not convert parameter to int: " + param.second);
            }
            else
            {
                ErpFail(HttpStatus::BadRequest,
                        "unknown parameter in request body: " + param.first + "=" + param.second);
            }
        }
    }
    ErpExpect(delay >= 0, HttpStatus::BadRequest, "Delay value " + std::to_string(delay) + " is out of range");
    auto& ioContext = session.serviceContext.getAdminServer().getThreadPool().ioContext();
    TerminationHandler::instance().gracefulShutdown(ioContext, delay);
    session.response.setBody("shutdown in " + std::to_string(delay) + " seconds");
}

Operation PostRestartHandler::getOperation(void) const
{
    return Operation::POST_Admin_restart;
}
