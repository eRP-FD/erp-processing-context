#include "HealthHandler.hxx"
#include "erp/database/Database.hxx"
#include "erp/database/RedisClient.hxx"
#include "erp/model/Health.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/response/ResponseBuilder.hxx"
#include "erp/server/response/ServerResponse.hxx"

namespace
{
void logUp(const std::string_view& checkName)
{
    TVLOG(2) << "health-check " << checkName << ": UP";
}
void logSkip(const std::string_view& checkName, const std::string_view& skipReason)
{
    TVLOG(2) << "health-check " << checkName << ": SKIPPED (" << skipReason << ")";
}
void logDown(const std::string_view& checkName, const std::string_view& rootCause)
{
    TVLOG(2) << "health-check " << checkName << ": DOWN (" << rootCause << ")";
}

}

void HealthHandler::handleRequest(SessionContext<PcServiceContext>& session)
{
    session.accessLog.setInnerRequestOperation("/health");

    model::Health health;

    bool postgresUp = checkPostgres(session, health);
    bool hsmUp = checkHsm(session, health);
    bool redisUp = checkRedis(session, health);
    bool tslUp = checkTsl(session, health);
    bool bnaUp = checkBna(session, health);
    bool idpUp = checkIdp(session, health);
    bool seedTimerUp = checkSeedTimer(session, health);
    bool teeTokenUpdaterUp = checkTeeTokenUpdater(session, health);

    if (postgresUp && hsmUp && redisUp && tslUp && bnaUp && idpUp && seedTimerUp && teeTokenUpdaterUp)
    {
        logUp("overall");
        health.setOverallStatus(model::Health::up);
    }
    else
    {
        logDown("overall", "one or more checks failed");
        TVLOG(1) << health.serializeToJsonString();
        health.setOverallStatus(model::Health::down);
    }

    ResponseBuilder(session.response).status(HttpStatus::OK).jsonBody(health.serializeToJsonString()).keepAlive(true);
}

Operation HealthHandler::getOperation(void) const
{
    return Operation::GET_Health;
}

bool HealthHandler::checkPostgres(SessionContext<PcServiceContext>& session, model::Health& health)
{
    try
    {
        session.database()->healthCheck();
        session.database()->commitTransaction();

        logUp(model::Health::postgres);
        health.setPostgresStatus(model::Health::up);
        return true;
    }
    catch (...)
    {
        handleException(std::current_exception(), model::Health::postgres,
            [&health](std::optional<std::string_view> rootCause) {
              health.setPostgresStatus(model::Health::down, rootCause);
            });
    }
    return false;
}

bool HealthHandler::checkHsm(SessionContext<PcServiceContext>& session, model::Health& health)
{
    const auto device = Configuration::instance().getStringValue(ConfigurationKey::HSM_DEVICE);
    try
    {
        auto hsmSession = session.serviceContext.getHsmPool().acquire();
        (void) hsmSession.session().getRandomData(1);

        logUp(model::Health::hsm);
        health.setHsmStatus(model::Health::up, device);
        return true;
    }
    catch (...)
    {
        handleException(std::current_exception(), model::Health::hsm,
            [&health, &device](std::optional<std::string_view> rootCause) {
              health.setHsmStatus(model::Health::down, device, rootCause);
            });
    }
    return false;
}

bool HealthHandler::checkRedis(SessionContext<PcServiceContext>& session, model::Health& health)
{
    try
    {
        if (! Configuration::instance().getOptionalBoolValue(ConfigurationKey::DEBUG_DISABLE_DOS_CHECK, false))
        {
            session.serviceContext.getDosHandler().healthCheck();
            logUp(model::Health::redis);
            health.setRedisStatus(model::Health::up);
        }
        else
        {
            logSkip(model::Health::redis, "DEBUG_DISABLE_DOS_CHECK=true");
            health.setRedisStatus(model::Health::up, "check skipped, DEBUG_DISABLE_DOS_CHECK=true");
        }
        return true;
    }
    catch (...)
    {
        handleException(std::current_exception(), model::Health::redis,
            [&health](std::optional<std::string_view> rootCause) {
              health.setRedisStatus(model::Health::down, rootCause);
            });
    }
    return false;
}

bool HealthHandler::checkBna(SessionContext<PcServiceContext>& session, model::Health& health)
{
    try
    {
        Expect(session.serviceContext.getTslManager() != nullptr, "TslManager must be set.");
        session.serviceContext.getTslManager()->healthCheckBna();
        logUp(model::Health::bna);
        health.setBnaStatus(model::Health::up);
        return true;
    }
    catch (...)
    {
        handleException(std::current_exception(), model::Health::bna,
                        [&health](std::optional<std::string_view> rootCause) {
                            health.setBnaStatus(model::Health::down, rootCause);
                        });
    }
    return false;
}

bool HealthHandler::checkTsl(SessionContext<PcServiceContext>& session, model::Health& health)
{
    try
    {
        Expect(session.serviceContext.getTslManager() != nullptr, "TslManager must be set.");
        session.serviceContext.getTslManager()->healthCheckTsl();
        logUp(model::Health::tsl);
        health.setTslStatus(model::Health::up);
        return true;
    }
    catch (...)
    {
        handleException(std::current_exception(), model::Health::tsl,
                        [&health](std::optional<std::string_view> rootCause) {
                            health.setTslStatus(model::Health::down, rootCause);
                        });
    }
    return false;
}

bool HealthHandler::checkIdp(SessionContext<PcServiceContext>& session, model::Health& health)
{
    try
    {
        session.serviceContext.idp.healthCheck();
        logUp(model::Health::idp);
        health.setIdpStatus(model::Health::up);
        return true;
    }
    catch (...)
    {
        handleException(std::current_exception(), model::Health::idp,
            [&health](std::optional<std::string_view> rootCause) {
              health.setIdpStatus(model::Health::down, rootCause);
            });
    }
    return false;
}

bool HealthHandler::checkSeedTimer(SessionContext<PcServiceContext>& session, model::Health& health)
{
    try
    {
        session.serviceContext.getPrngSeeder()->healthCheck();
        logUp(model::Health::seedTimer);
        health.setSeedTimerStatus(model::Health::up);
        return true;
    }
    catch (...)
    {
        handleException(std::current_exception(), model::Health::seedTimer,
            [&health](std::optional<std::string_view> rootCause) {
              health.setSeedTimerStatus(model::Health::down, rootCause);
            });
    }
    return false;
}

bool HealthHandler::checkTeeTokenUpdater(SessionContext<PcServiceContext>& session, model::Health& health)
{
    try
    {
        session.serviceContext.getHsmPool().getTokenUpdater().healthCheck();
        logUp(model::Health::teeTokenUpdater);
        health.setTeeTokenUpdaterStatus(model::Health::up);
        return true;
    }
    catch (...)
    {
        handleException(std::current_exception(), model::Health::teeTokenUpdater,
            [&health](std::optional<std::string_view> rootCause) {
              health.setTeeTokenUpdaterStatus(model::Health::down, rootCause);
            });
    }
    return false;
}

void HealthHandler::handleException(const std::exception_ptr& exceptionPtr, const std::string_view& checkName,
                                    const std::function<void(std::optional<std::string_view>)>& statusSetter)
{
    try
    {
        std::rethrow_exception(exceptionPtr);
    }
    catch (const std::exception& ex)
    {
        logDown(checkName, ex.what());
        statusSetter(ex.what());
    }
    catch (...)
    {
        logDown(checkName, "unknown error");
        statusSetter({});
    }
}
