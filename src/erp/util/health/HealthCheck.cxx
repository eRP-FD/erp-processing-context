/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/health/HealthCheck.hxx"
#include "erp/database/DatabaseConnectionInfo.hxx"
#include "erp/pc/SeedTimer.hxx"
#include "erp/registration/RegistrationInterface.hxx"
#include "erp/util/Demangle.hxx"
#include "erp/util/ExceptionHelper.hxx"

namespace
{

void setTslDetails(ApplicationHealth::Service service, const TrustStore::HealthData& healthData,
                   ApplicationHealth& applicationHealth)
{
    applicationHealth.setServiceDetails(service, ApplicationHealth::ServiceDetail::TSLExpiry,
                                        model::Timestamp(healthData.nextUpdate).toXsDateTime());
    applicationHealth.setServiceDetails(service, ApplicationHealth::ServiceDetail::TSLSequenceNumber,
                                        healthData.sequenceNumber);
    applicationHealth.setServiceDetails(service, ApplicationHealth::ServiceDetail::TSLId, healthData.id.value_or(""));
    applicationHealth.setServiceDetails(service, ApplicationHealth::ServiceDetail::TslHash, String::toHexString(healthData.hash.value_or("")));
}

}// namespace


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

    const auto healthDataTsl = context.getTslManager().healthCheckTsl();
    setTslDetails(ApplicationHealth::Service::Tsl, healthDataTsl, context.applicationHealth());
    const auto healthDataBna = context.getTslManager().healthCheckBna();
    setTslDetails(ApplicationHealth::Service::Bna, healthDataBna, context.applicationHealth());

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
        std::string typeinfo = util::demangle(typeid(err).name());
        TLOG(WARNING) << "Could not update registration status due to exception (" << typeinfo << "): " << err.what();
    }
}


void HealthCheck::checkBna (PcServiceContext& context)
{
    const auto healthData = context.getTslManager().healthCheckBna();
    Expect3(healthData.hasTsl, "No BNetzA loaded", std::runtime_error);
    Expect3(! healthData.outdated, "Loaded BNetzA is too old", std::runtime_error);
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


void HealthCheck::checkPostgres(PcServiceContext& context)
{
    auto connection = context.databaseFactory();
    connection->healthCheck();
    auto connectionInfo = connection->getConnectionInfo();
    connection->commitTransaction();
    if (connectionInfo)
    {
        context.applicationHealth().setServiceDetails(ApplicationHealth::Service::Postgres,
                                                      ApplicationHealth::ServiceDetail::DBConnectionInfo,
                                                      toString(*connectionInfo));
    }
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
    const auto healthData = context.getTslManager().healthCheckTsl();
    Expect3(healthData.hasTsl, "No TSL loaded", std::runtime_error);
    Expect3(! healthData.outdated, "Loaded TSL is too old", std::runtime_error);
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
