/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/MetaDataHandler.hxx"

#include "erp/model/MetaData.hxx"
#include "shared/util/TLog.hxx"


MetaDataHandler::MetaDataHandler ()
    : ErpUnconstrainedRequestHandler(Operation::GET_metadata)
{
}

void MetaDataHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const model::MetaData metaData{};
    makeResponse(session, HttpStatus::OK, &metaData);
}
