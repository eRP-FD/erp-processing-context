/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "exporter/admin/PutRuntimeConfigHandler.hxx"
#include "exporter/network/client/HttpsClientPool.hxx"
#include "exporter/network/client/Tee3ClientPool.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "exporter/server/HealthHandler.hxx"
#include "exporter/util/ConfigurationFormatter.hxx"
#include "exporter/util/RuntimeConfiguration.hxx"
#include "shared/admin/AdminRequestHandler.hxx"
#include "shared/enrolment/EnrolmentServer.hxx"
#include "shared/util/Configuration.hxx"

#include <date/date.h>

namespace {
std::chrono::steady_clock::duration refreshInterval() {
    std::chrono::steady_clock::duration result;
    std::istringstream intervalStr{Configuration::instance().getStringValue(
        ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_DNS_REFRESH_INTERVAL)};
    intervalStr.imbue(std::locale::classic());
    intervalStr >> date::parse("%H:%M:%S", result);
    Expect3(! intervalStr.fail() && intervalStr.peek() == std::char_traits<char>::eof(),
            "Invalid format for MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_DNS_REFRESH_INTERVAL expected HH:MM:SS", std::logic_error);
    return result;
}
}

MedicationExporterServiceContext::MedicationExporterServiceContext(boost::asio::io_context& ioContext,
                                                                   const Configuration& configuration,
                                                                   const MedicationExporterFactories& factories)
    : BaseServiceContext(factories)
    , mIoContext{ioContext}
    , mExporterDatabaseFactory(factories.exporterDatabaseFactory)
    , mErpDatabaseFactory(factories.erpDatabaseFactory)
    , mTeeClientPool{Tee3ClientPool::create(ioContext, *mHsmPool, *mTslManager, refreshInterval())}
    , mRuntimeConfiguration(std::make_unique<exporter::RuntimeConfiguration>())
{
    Expect3(mExporterDatabaseFactory != nullptr,
            "exporter database factory has been passed as nullptr to ServiceContext constructor", std::logic_error);
    Expect3(mErpDatabaseFactory != nullptr,
            "erp database factory has been passed as nullptr to ServiceContext constructor", std::logic_error);

    using enum ApplicationHealth::Service;
    applicationHealth().enableChecks({Bna, Hsm, Postgres, PrngSeed, TeeToken, Tsl, EventDb});

    RequestHandlerManager adminHandlers;
    adminHandlers.onGetDo("/health", std::make_unique<exporter::HealthHandler>());
    adminHandlers.onPostDo("/admin/shutdown",
                           std::make_unique<PostRestartHandler>(
                               ConfigurationKey::MEDICATION_EXPORTER_ADMIN_CREDENTIALS,
                               ConfigurationKey::MEDICATION_EXPORTER_ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS));
    adminHandlers.onGetDo("/admin/configuration",
                          std::make_unique<GetConfigurationHandler>(
                              ConfigurationKey::MEDICATION_EXPORTER_ADMIN_CREDENTIALS,
                              std::make_unique<exporter::ConfigurationFormatter>(getRuntimeConfiguration())));
    adminHandlers.onPutDo("/admin/configuration", std::make_unique<exporter::PutRuntimeConfigHandler>(
                                                      ConfigurationKey::MEDICATION_EXPORTER_ADMIN_RC_CREDENTIALS));
    mAdminServer = factories.adminServerFactory(
        configuration.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_ADMIN_SERVER_INTERFACE),
        gsl::narrow<uint16_t>(configuration.getIntValue(ConfigurationKey::MEDICATION_EXPORTER_ADMIN_SERVER_PORT)),
        std::move(adminHandlers), *this, false, SafeString{});

    auto enrolmentServerPort =
        getEnrolementServerPort(configuration.serverPort(), EnrolmentServer::DefaultEnrolmentServerPort);
    if (enrolmentServerPort)
    {
        RequestHandlerManager enrolmentHandlers;
        EnrolmentServer::addEndpoints(enrolmentHandlers);
        mEnrolmentServer = factories.enrolmentServerFactory(BaseHttpsServer::defaultHost, *enrolmentServerPort,
                                                            std::move(enrolmentHandlers), *this, false, SafeString{});
    }

    auto fqdns = configuration.epaFQDNs();

    TlsCertificateVerifier verifier =
        TlsCertificateVerifier::withTslVerification(getTslManager(),
                                                    {.certificateType = CertificateType::C_FD_TLS_S,
                                                     .ocspGracePeriod = std::chrono::seconds(configuration.getIntValue(
                                                         ConfigurationKey::MEDICATION_EXPORTER_OCSP_EPA_GRACE_PERIOD)),
                                                     .withSubjectAlternativeAddressValidation = true})
            .withAdditionalCertificateCheck([](const X509Certificate& cert) -> bool {
                return cert.checkRoles({std::string{profession_oid::oid_epa_dvw}});
            });
    std::vector<std::tuple<std::string, uint16_t>> hostPortList;
    auto poolSize = static_cast<size_t>(
        configuration.getIntValue(ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_POOL_SIZE_PER_FQDN));
    for (const auto& fqdn : fqdns)
    {
        ConnectionParameters params{
            .hostname = fqdn.hostName,
            .port = std::to_string(fqdn.port),
            .connectionTimeout = std::chrono::seconds{configuration.getIntValue(
              ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_CONNECT_TIMEOUT_SECONDS)},
            .resolveTimeout = std::chrono::milliseconds{configuration.getIntValue(
              ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_RESOLVE_TIMEOUT_MILLISECONDS)},
            .tlsParameters = TlsConnectionParameters{.certificateVerifier = verifier}
        };
        mHttpsClientPools.emplace(fqdn.hostName,
                                  HttpsClientPool::create(&mIoContext, std::move(params), poolSize, refreshInterval()));
    }
}

MedicationExporterServiceContext::~MedicationExporterServiceContext() = default;

std::unique_ptr<MedicationExporterDatabaseFrontendInterface>
MedicationExporterServiceContext::medicationExporterDatabaseFactory(TransactionMode mode)
{
    return mExporterDatabaseFactory(mKeyDerivation, mode);
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


std::shared_ptr<HttpsClientPool> MedicationExporterServiceContext::httpsClientPool(const std::string& hostname)
{
    return mHttpsClientPools.at(hostname);
}


std::unique_ptr<exporter::RuntimeConfigurationGetter>
MedicationExporterServiceContext::getRuntimeConfigurationGetter() const
{
    return std::make_unique<exporter::RuntimeConfigurationGetter>(mRuntimeConfiguration);
}

std::unique_ptr<exporter::RuntimeConfigurationSetter>
MedicationExporterServiceContext::getRuntimeConfigurationSetter() const
{
    return std::make_unique<exporter::RuntimeConfigurationSetter>(mRuntimeConfiguration);
}

std::shared_ptr<const exporter::RuntimeConfiguration> MedicationExporterServiceContext::getRuntimeConfiguration() const
{
    return mRuntimeConfiguration;
}
