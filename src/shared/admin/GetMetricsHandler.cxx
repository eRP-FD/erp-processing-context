// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "GetMetricsHandler.hxx"
#include "shared/beast/BoostBeastStringWriter.hxx"
#include "shared/server/BaseServiceContext.hxx"
#include "shared/util/MetricsRegistry.hxx"

Operation GetMetricsHandler::getOperation() const
{
    return Operation::GET_Admin_metrics;
}

void GetMetricsHandler::handleRequest(BaseSessionContext& session)
{
    try
    {
        TVLOG(2) << BoostBeastStringWriter::serializeRequest(session.request.header(), session.request.getBody());
        auto body = MetricsRegistry::instance().serialize();
        session.response.setBody(std::move(body));
        session.response.setHeader(Header::ContentType, "text/plain; charset=utf-8");
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
