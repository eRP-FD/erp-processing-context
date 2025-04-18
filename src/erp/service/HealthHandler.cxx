/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/HealthHandler.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "shared/server/response/ResponseBuilder.hxx"
#include "erp/util/health/HealthCheck.hxx"


void HealthHandler::handleRequest(BaseSessionContext& baseSessionContext)
{
    auto& session = dynamic_cast<PcSessionContext&>(baseSessionContext);
    session.accessLog.setInnerRequestOperation("/health");

    std::stringstream errorStream;
    std::optional<model::Health> healthResource;
    try
    {
        HealthCheck::update(session.serviceContext);
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
        session.accessLog.error(errorStream.str());
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
