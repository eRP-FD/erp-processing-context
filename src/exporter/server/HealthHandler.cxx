#include "HealthHandler.hxx"
#include "exporter/util/health/HealthCheck.hxx"
#include "shared/model/Health.hxx"
#include "shared/server/response/ResponseBuilder.hxx"


void HealthHandler::handleRequest(SessionContext& session)
{
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

    if (! healthResource)
    {
        healthResource = model::Health();
    }
    if (! errorStream.str().empty())
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
