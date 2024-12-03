/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/DeviceHandler.hxx"

#include "shared/ErpRequirements.hxx"
#include "erp/model/Device.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/util/TLog.hxx"


DeviceHandler::DeviceHandler()
    : ErpUnconstrainedRequestHandler(Operation::GET_Device)
{
}

void DeviceHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    A_20744.start("Self-disclosure device information");

    static const model::Device device;
    makeResponse(session, HttpStatus::OK, &device);

    A_20744.finish();
}
