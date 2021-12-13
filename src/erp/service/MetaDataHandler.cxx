/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

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

    const model::MetaData metaData;
    makeResponse(session, HttpStatus::OK, &metaData);
}

