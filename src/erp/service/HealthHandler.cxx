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

    std::stringstream errorStream;
    std::optional<model::Health> healthResource;
    try
    {
        HealthCheck::update(session);
        healthResource = session.serviceContext.applicationHealth().model();
    }
    catch (const std::exception& ex)
    {
        errorStream << "exception during health check update: " << ex.what();
    }
    catch (...)
    {
        errorStream << "unknown exception during health check update";
    }

    if (!healthResource)
    {
        healthResource = model::Health();
    }
    if (!errorStream.str().empty())
    {
        healthResource->setHealthCheckError(errorStream.str());
        healthResource->setOverallStatus(model::Health::down);
    }

    ResponseBuilder(session.response)
        .status(HttpStatus::OK)
        .jsonBody(healthResource->serializeToJsonString())
        .keepAlive(true);
}

Operation HealthHandler::getOperation(void) const
{
    return Operation::GET_Health;
}
