/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/HealthHandler.hxx"

#include "erp/server/context/SessionContext.hxx"
#include "erp/server/response/ResponseBuilder.hxx"
#include "erp/util/health/HealthCheck.hxx"


void HealthHandler::handleRequest (SessionContext<PcServiceContext>& session)
{
    session.accessLog.setInnerRequestOperation("/health");

    HealthCheck::update(session);

    ResponseBuilder(session.response)
        .status(HttpStatus::OK)
        .jsonBody(session.serviceContext.applicationHealth().model().serializeToJsonString())
        .keepAlive(true);
}

Operation HealthHandler::getOperation(void) const
{
    return Operation::GET_Health;
}
