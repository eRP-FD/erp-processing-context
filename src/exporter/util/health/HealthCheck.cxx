/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/util/health/HealthCheck.hxx"
#include "erp/database/Database.hxx"
#include "shared/database/DatabaseConnectionInfo.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/ExceptionHelper.hxx"
#include "shared/util/health/ApplicationHealth.hxx"

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
    applicationHealth.setServiceDetails(service, ApplicationHealth::ServiceDetail::TslHash,
                                        String::toHexString(healthData.hash.value_or("")));
}

}// namespace


void HealthCheck::update(MedicationExporterServiceContext& context)
{
    check(ApplicationHealth::Service::Postgres, context, &checkPostgres);

    check(ApplicationHealth::Service::EventDb, context, &checkEventDb);

    check(ApplicationHealth::Service::Hsm, context, &checkHsm);
    context.applicationHealth().setServiceDetails(
        ApplicationHealth::Service::Hsm, ApplicationHealth::ServiceDetail::HsmDevice,
        Configuration::instance().getStringValue(ConfigurationKey::HSM_DEVICE));

    check(ApplicationHealth::Service::Tsl, context, &checkTsl);
    const auto healthDataTsl = context.getTslManager().healthCheckTsl();
    setTslDetails(ApplicationHealth::Service::Tsl, healthDataTsl, context.applicationHealth());

    check(ApplicationHealth::Service::Bna, context, &checkBna);
	const auto healthDataBna = context.getTslManager().healthCheckBna();
	setTslDetails(ApplicationHealth::Service::Bna, healthDataBna, context.applicationHealth());

    check(ApplicationHealth::Service::PrngSeed,     context, &checkSeedTimer);

    check(ApplicationHealth::Service::TeeToken, context, &checkTeeTokenUpdater);
}


void HealthCheck::checkBna(MedicationExporterServiceContext& context)
{
    const auto healthData = context.getTslManager().healthCheckBna();
    Expect3(healthData.hasTsl, "No BNetzA loaded", std::runtime_error);
    Expect3(! healthData.outdated, "Loaded BNetzA is too old", std::runtime_error);
}


void HealthCheck::checkHsm(MedicationExporterServiceContext& context)
{
    auto hsmSession = context.getHsmPool().acquire();
    (void) hsmSession.session().getRandomData(1);
}


void HealthCheck::checkPostgres(MedicationExporterServiceContext& context)
{
    auto connection = context.erpDatabaseFactory();
    auto connectionInfo = connection->getConnectionInfo();
    connection->healthCheck();
    connection->commitTransaction();
    if (connectionInfo)
    {
        context.applicationHealth().setServiceDetails(ApplicationHealth::Service::Postgres,
                                                      ApplicationHealth::ServiceDetail::DBConnectionInfo,
                                                      toString(*connectionInfo));
    }
}


void HealthCheck::checkSeedTimer(MedicationExporterServiceContext& context)
{
    (void) context;
    // TODO(walantis) prng seeder not yet available
}


void HealthCheck::checkTeeTokenUpdater(MedicationExporterServiceContext& context)
{
    context.getHsmPool().getTokenUpdater().healthCheck();
}


void HealthCheck::checkTsl(MedicationExporterServiceContext& context)
{
    const auto healthData = context.getTslManager().healthCheckTsl();
    Expect3(healthData.hasTsl, "No TSL loaded", std::runtime_error);
    Expect3(! healthData.outdated, "Loaded TSL is too old", std::runtime_error);
}


void HealthCheck::checkEventDb(MedicationExporterServiceContext& context)
{
    auto connection = context.medicationExporterDatabaseFactory();
    connection->healthCheck();
    auto connectionInfo = connection->getConnectionInfo();
    connection->commitTransaction();
    if (connectionInfo)
    {
        context.applicationHealth().setServiceDetails(ApplicationHealth::Service::EventDb,
                                                      ApplicationHealth::ServiceDetail::DBConnectionInfo,
                                                      toString(*connectionInfo));
    }
}


void HealthCheck::check(const ApplicationHealth::Service service, MedicationExporterServiceContext& context,
                        void (*checkAction)(MedicationExporterServiceContext&))
{
    // Try again in case of EAGAIN errors, which are handled with the same exception as ETIMEDOUT and EWOULDBLOCK errors.
    const size_t max_retries = 3;
    for (size_t i = 0; i <= max_retries; ++i)
    {
        try
        {
            (*checkAction)(context);
            context.applicationHealth().up(service);
            break;
        }
        catch (...)
        {
            handleException(service, context);
        }
    }
}


void HealthCheck::handleException(ApplicationHealth::Service service, MedicationExporterServiceContext& context)
{
    ExceptionHelper::extractInformation(
        [service, &context](std::string&& details, std::string&& location) {
            context.applicationHealth().down(service, details + " at " + location);
        },
        std::current_exception());
}
