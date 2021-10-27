/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/health/HealthCheck.hxx"

#include "erp/server/context/SessionContext.hxx"
#include "erp/util/ExceptionHelper.hxx"


void HealthCheck::update (SessionContext<PcServiceContext>& session)
{
    check(ApplicationHealth::Service::Bna,      session, &checkBna);
    check(ApplicationHealth::Service::Hsm,      session, &checkHsm);
    check(ApplicationHealth::Service::Idp,      session, &checkIdp);
    check(ApplicationHealth::Service::Postgres, session, &checkPostgres);
    check(ApplicationHealth::Service::PrngSeed, session, &checkSeedTimer);
    check(ApplicationHealth::Service::TeeToken, session, &checkTeeTokenUpdater);
    check(ApplicationHealth::Service::Tsl,      session, &checkTsl);

    if (Configuration::instance().getOptionalBoolValue(ConfigurationKey::DEBUG_DISABLE_DOS_CHECK, false))
        session.serviceContext.applicationHealth().skip(
            ApplicationHealth::Service::Redis,
            "DEBUG_DISABLE_DOS_CHECK=true");
    else
        check(ApplicationHealth::Service::Redis, session, &checkRedis);

    session.serviceContext.applicationHealth().setServiceDetails(ApplicationHealth::Service::Hsm, Configuration::instance().getStringValue(ConfigurationKey::HSM_DEVICE));
}


void HealthCheck::checkBna (SessionContext<PcServiceContext>& session)
{
    Expect(session.serviceContext.getTslManager() != nullptr, "TslManager must be set.");
    session.serviceContext.getTslManager()->healthCheckBna();
}


void HealthCheck::checkHsm (SessionContext<PcServiceContext>& session)
{
    auto hsmSession = session.serviceContext.getHsmPool().acquire();
    (void) hsmSession.session().getRandomData(1);
}


void HealthCheck::checkIdp (SessionContext<PcServiceContext>& session)
{
    session.serviceContext.idp.healthCheck();
}


void HealthCheck::checkPostgres (SessionContext<PcServiceContext>& session)
{
    session.database()->healthCheck();
    session.database()->commitTransaction();
}


void HealthCheck::checkRedis (SessionContext<PcServiceContext>& session)
{
    session.serviceContext.getDosHandler().healthCheck();
}


void HealthCheck::checkSeedTimer (SessionContext<PcServiceContext>& session)
{
    session.serviceContext.getPrngSeeder()->healthCheck();
}


void HealthCheck::checkTeeTokenUpdater (SessionContext<PcServiceContext>& session)
{
    session.serviceContext.getHsmPool().getTokenUpdater().healthCheck();
}


void HealthCheck::checkTsl (SessionContext<PcServiceContext>& session)
{
    Expect(session.serviceContext.getTslManager() != nullptr, "TslManager must be set.");
    session.serviceContext.getTslManager()->healthCheckTsl();
}


void HealthCheck::check (
    const ApplicationHealth::Service service,
    SessionContext<PcServiceContext>& session,
    void (* checkAction)(SessionContext<PcServiceContext>&))
{
    try
    {
        (*checkAction)(session);

        session.serviceContext.applicationHealth().up(service);
    }
    catch (...)
    {
        handleException(service, session);
    }
}


void HealthCheck::handleException(
    ApplicationHealth::Service service,
    SessionContext<PcServiceContext>& session)
{
    ExceptionHelper::extractInformation(
        [service, &session]
        (std::string&& details, std::string&& location)
        {
            session.serviceContext.applicationHealth().down(service, details + " at " + location);
        },
        std::current_exception());
}
