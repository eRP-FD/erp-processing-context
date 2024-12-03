/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "exporter/network/client/Tee3ClientPool.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "exporter/server/HealthHandler.hxx"
#include "shared/enrolment/EnrolmentServer.hxx"
#include "shared/util/Configuration.hxx"

#include <boost/asio/io_context.hpp>

MedicationExporterServiceContext::MedicationExporterServiceContext(boost::asio::io_context& ioContext,
                                                                   const Configuration& configuration,
                                                                   MedicationExporterFactories&& factories)
    : BaseServiceContext(factories)
    , mIoContext{ioContext}
    , mExporterDatabaseFactory(std::move(factories.exporterDatabaseFactory))
    , mErpDatabaseFactory(std::move(factories.erpDatabaseFactory))
    , mTeeClientPool{std::make_shared<Tee3ClientPool>(
          ioContext,
          static_cast<size_t>(configuration.getIntValue(ConfigurationKey::MEDICATION_EXPORTER_SERVER_IO_THREAD_COUNT)),
          *mHsmPool, *mTslManager)}
{
    Expect3(mExporterDatabaseFactory != nullptr,
            "exporter database factory has been passed as nullptr to ServiceContext constructor", std::logic_error);
    Expect3(mErpDatabaseFactory != nullptr,
            "erp database factory has been passed as nullptr to ServiceContext constructor", std::logic_error);

    using enum ApplicationHealth::Service;
    applicationHealth().enableChecks({Bna, Hsm, Postgres, PrngSeed, TeeToken, Tsl, EventDb});

    RequestHandlerManager adminHandlers;
    adminHandlers.onGetDo("/health", std::make_unique<HealthHandler>());
    mAdminServer = factories.adminServerFactory(
        configuration.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_ADMIN_SERVER_INTERFACE),
        gsl::narrow<uint16_t>(configuration.getIntValue(ConfigurationKey::MEDICATION_EXPORTER_ADMIN_SERVER_PORT)), std::move(adminHandlers),
        *this, false, SafeString{});

    auto enrolmentServerPort =
        getEnrolementServerPort(configuration.serverPort(), EnrolmentServer::DefaultEnrolmentServerPort);
    if (enrolmentServerPort)
    {
        RequestHandlerManager enrolmentHandlers;
        EnrolmentServer::addEndpoints(enrolmentHandlers);
        mEnrolmentServer = factories.enrolmentServerFactory(BaseHttpsServer::defaultHost, *enrolmentServerPort,
                                                            std::move(enrolmentHandlers), *this, false, SafeString{});
    }
}

std::unique_ptr<MedicationExporterDatabaseFrontendInterface>
MedicationExporterServiceContext::medicationExporterDatabaseFactory()
{
    return mExporterDatabaseFactory(*mHsmPool, mKeyDerivation);
}

std::unique_ptr<exporter::MainDatabaseFrontend> MedicationExporterServiceContext::erpDatabaseFactory()
{
    return mErpDatabaseFactory(*mHsmPool, mKeyDerivation);
}

boost::asio::io_context& MedicationExporterServiceContext::ioContext()
{
    return mIoContext;
}


std::shared_ptr<Tee3ClientPool> MedicationExporterServiceContext::teeClientPool()
{
    return mTeeClientPool;
}
