#include "erp/service/MetaDataHandler.hxx"

#include "erp/model/MetaData.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/util/TLog.hxx"


MetaDataHandler::MetaDataHandler ()
    : ErpUnconstrainedRequestHandler(Operation::GET_metadata)
{
}

void MetaDataHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    static const model::MetaData metaData;
    makeResponse(session, HttpStatus::OK, &metaData);
}

