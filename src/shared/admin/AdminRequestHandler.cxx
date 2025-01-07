/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/admin/AdminRequestHandler.hxx"
#include "shared/beast/BoostBeastStringWriter.hxx"
#include "shared/deprecated/TerminationHandler.hxx"
#include "shared/server/BaseServiceContext.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/ConfigurationFormatter.hxx"
#include "shared/util/TLog.hxx"
#include "shared/util/UrlHelper.hxx"

AdminRequestHandlerBase::AdminRequestHandlerBase(ConfigurationKey credentialsKey)
    : mCredentialsKey(credentialsKey)
{
}

void AdminRequestHandlerBase::handleRequest(BaseSessionContext& session)
{
    try
    {
        TVLOG(2) << BoostBeastStringWriter::serializeRequest(session.request.header(), session.request.getBody());
        handleBasicAuthentication(session, mCredentialsKey);
        doHandleRequest(session);
        session.response.setStatus(HttpStatus::OK);
    }
    catch (const ErpException& ex)
    {
        session.response.setStatus(ex.status());
        session.response.setBody(ex.what());
    }
    catch (const model::ModelException& me)
    {
        session.response.setStatus(HttpStatus::BadRequest);
        session.response.setBody(me.what());
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

PostRestartHandler::PostRestartHandler(ConfigurationKey adminCredentialsKey,
                                       ConfigurationKey adminDefaultShutdownDelayKey)
    : AdminRequestHandlerBase(adminCredentialsKey)
    , mDefaultShutdownDelay(Configuration::instance().getIntValue(adminDefaultShutdownDelayKey))
{
}

PostRestartHandler::~PostRestartHandler() = default;

void PostRestartHandler::doHandleRequest(BaseSessionContext& session)
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
    auto& ioContext = session.baseServiceContext.getAdminServer().getThreadPool().ioContext();
    TerminationHandler::instance().gracefulShutdown(ioContext, delay);
    session.response.setBody("shutdown in " + std::to_string(delay) + " seconds");
}

GetConfigurationHandler::GetConfigurationHandler(ConfigurationKey adminCredentialsKey,
                                                 gsl::not_null<std::unique_ptr<ConfigurationFormatter>> formatter)
    : AdminRequestHandlerBase(adminCredentialsKey)
    , mFormatter(std::move(formatter))
{
}

Operation PostRestartHandler::getOperation(void) const
{
    return Operation::POST_Admin_restart;
}

void GetConfigurationHandler::doHandleRequest(BaseSessionContext& session)
{
    TVLOG(1) << "configuration requested by: " << session.request.header().serializeFields();
    const auto& config = Configuration::instance();
    using Flags = KeyData::ConfigurationKeyFlags;
    session.response.setBody(mFormatter->formatAsJson(config, Flags::all));
    session.response.setHeader(Header::ContentType, MimeType::json);
}

Operation GetConfigurationHandler::getOperation() const
{
    return Operation::GET_Admin_configuration;
}
