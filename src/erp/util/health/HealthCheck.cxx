/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/health/HealthCheck.hxx"

#include "erp/pc/SeedTimer.hxx"
#include "erp/util/ExceptionHelper.hxx"
#include "erp/registration/RegistrationInterface.hxx"


void HealthCheck::update (PcServiceContext& context)
{
    check(ApplicationHealth::Service::Bna,          context, &checkBna);
    check(ApplicationHealth::Service::Hsm,          context, &checkHsm);
    check(ApplicationHealth::Service::Idp,          context, &checkIdp);
    check(ApplicationHealth::Service::Postgres,     context, &checkPostgres);
    check(ApplicationHealth::Service::PrngSeed,     context, &checkSeedTimer);
    check(ApplicationHealth::Service::TeeToken,     context, &checkTeeTokenUpdater);
    check(ApplicationHealth::Service::Tsl,          context, &checkTsl);
    check(ApplicationHealth::Service::CFdSigErp,    context, &checkCFdSigErp);

    if (Configuration::instance().getOptionalBoolValue(ConfigurationKey::DEBUG_DISABLE_DOS_CHECK, false))
        context.applicationHealth().skip(
            ApplicationHealth::Service::Redis,
            "DEBUG_DISABLE_DOS_CHECK=true");
    else
        check(ApplicationHealth::Service::Redis, context, &checkRedis);

    context.applicationHealth().setServiceDetails(
        ApplicationHealth::Service::Hsm, ApplicationHealth::ServiceDetail::HsmDevice,
        Configuration::instance().getStringValue(ConfigurationKey::HSM_DEVICE));

    try
    {
        context.applicationHealth().setServiceDetails(
            ApplicationHealth::Service::CFdSigErp, ApplicationHealth::ServiceDetail::CFdSigErpTimestamp,
            "last success " + context.getCFdSigErpManager().getLastOcspResponseTimestamp());
        context.applicationHealth().setServiceDetails(
            ApplicationHealth::Service::CFdSigErp, ApplicationHealth::ServiceDetail::CFdSigErpPolicy,
            String::replaceAll(
                std::string(magic_enum::enum_name(context.getCFdSigErpManager().getCertificateType())),
                "_", "."));
        context.applicationHealth().setServiceDetails(
            ApplicationHealth::Service::CFdSigErp, ApplicationHealth::ServiceDetail::CFdSigErpExpiry,
            context.getCFdSigErpManager().getCertificateNotAfterTimestamp());
    }
    catch (const std::exception& err)
    {
        TLOG(WARNING) << "Could not retrieve C.FD.SIG health service details due to exception: " << err.what();
    }

    try
    {
        context.registrationInterface()->updateRegistrationBasedOnApplicationHealth(
            context.applicationHealth());
    }
    catch (const std::exception& err)
    {
        TLOG(WARNING) << "Could not update registration status due to exception: " << err.what();
    }
}


void HealthCheck::checkBna (PcServiceContext& context)
{
    context.getTslManager().healthCheckBna();
}


void HealthCheck::checkHsm (PcServiceContext& context)
{
    auto hsmSession = context.getHsmPool().acquire();
    (void) hsmSession.session().getRandomData(1);
}


void HealthCheck::checkCFdSigErp (PcServiceContext& context)
{
    context.getCFdSigErpManager().healthCheck();
}


void HealthCheck::checkIdp (PcServiceContext& context)
{
    context.idp.healthCheck();
}


void HealthCheck::checkPostgres (PcServiceContext& context)
{
    auto connection = context.databaseFactory();
    connection->healthCheck();
    connection->commitTransaction();
}


void HealthCheck::checkRedis (PcServiceContext& context)
{
    context.getDosHandler().healthCheck();
}


void HealthCheck::checkSeedTimer (PcServiceContext& context)
{
    context.getPrngSeeder()->handler()->healthCheck();
}


void HealthCheck::checkTeeTokenUpdater (PcServiceContext& context)
{
    context.getHsmPool().getTokenUpdater().healthCheck();
}


void HealthCheck::checkTsl (PcServiceContext& context)
{
    context.getTslManager().healthCheckTsl();
}


void HealthCheck::check (
    const ApplicationHealth::Service service,
    PcServiceContext& context,
    void (* checkAction)(PcServiceContext&))
{
    try
    {
        (*checkAction)(context);

        context.applicationHealth().up(service);
    }
    catch (...)
    {
        handleException(service, context);
    }
}


void HealthCheck::handleException(
    ApplicationHealth::Service service,
    PcServiceContext& context)
{
    ExceptionHelper::extractInformation(
        [service, &context]
        (std::string&& details, std::string&& location)
        {
            context.applicationHealth().down(service, details + " at " + location);
        },
        std::current_exception());
}
