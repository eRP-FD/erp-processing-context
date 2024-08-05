/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/pc/PcServiceContext.hxx"
#include "erp/ErpProcessingContext.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/admin/AdminServer.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/database/Database.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/database/RedisClient.hxx"
#include "erp/enrolment/EnrolmentServer.hxx"
#include "erp/hsm/VsdmKeyBlobDatabase.hxx"
#include "erp/hsm/VsdmKeyCache.hxx"
#include "erp/pc/SeedTimer.hxx"
#include "erp/registration/RegistrationInterface.hxx"
#include "erp/registration/RegistrationManager.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/RuntimeConfiguration.hxx"
#include "erp/validation/JsonValidator.hxx"


namespace
{
std::unique_ptr<TslRefreshJob> setupTslRefreshJob(TslManager& tslManager, const Configuration& configuration)
{
    GS_A_4899.start("Create asynchronous TSL-Update job.");
    // 24 Hours per default
    const std::chrono::seconds tslRefreshInterval{configuration.getIntValue(ConfigurationKey::TSL_REFRESH_INTERVAL)};
    auto refreshJob = std::make_unique<TslRefreshJob>(tslManager, tslRefreshInterval);
    refreshJob->start();
    GS_A_4899.finish();
    return refreshJob;
}

std::unique_ptr<RateLimiter> createRateLimiter(std::shared_ptr<RedisInterface>& redisClient)
{
    const auto calls = gsl::narrow<size_t>(Configuration::instance().getIntValue(ConfigurationKey::TOKEN_ULIMIT_CALLS));
    const auto timespan =
        std::chrono::milliseconds(Configuration::instance().getIntValue(ConfigurationKey::TOKEN_ULIMIT_TIMESPAN_MS));
    return std::make_unique<RateLimiter>(redisClient, "ERP-PC-DOS", calls, timespan);
}
}


// GEMREQ-start A_20974-01
PcServiceContext::PcServiceContext(const Configuration& configuration, Factories&& factories)
    : idp()
    , mTimerManager(std::make_shared<Timer>())
    , mDatabaseFactory(std::move(factories.databaseFactory))
    , mRedisClient(factories.redisClientFactory(
          std::chrono::milliseconds(configuration.getIntValue(ConfigurationKey::REDIS_DOS_SOCKET_TIMEOUT))))
    , mDosHandler(createRateLimiter(mRedisClient))
    , mBlobCache(factories.blobCacheFactory())
    , mHsmPool(std::make_unique<HsmPool>(factories.hsmFactoryFactory(factories.hsmClientFactory(), mBlobCache),
                                         factories.teeTokenUpdaterFactory, mTimerManager))
    , mVsdmKeyCache(std::make_unique<VsdmKeyCache>(factories.vsdmKeyBlobDatabaseFactory(), *mHsmPool))
    , mKeyDerivation(*mHsmPool)
    , mJsonValidator(factories.jsonValidatorFactory())
    , mXmlValidator(factories.xmlValidatorFactory())
    , mPreUserPseudonymManager(PreUserPseudonymManager::create(this))
    , mTelematicPseudonymManager(TelematicPseudonymManager::create(this))
    , mTslManager(factories.tslManagerFactory(mXmlValidator))
    , mCFdSigErpManager(std::make_unique<CFdSigErpManager>(configuration, *mTslManager, *mHsmPool))
    , mTslRefreshJob(setupTslRefreshJob(*mTslManager, configuration))
    , mReportPseudonameKeyRefreshJob(
          PseudonameKeyRefreshJob::setupPseudonameKeyRefreshJob(*mHsmPool, *getBlobCache(), configuration))
    , mRegistrationInterface(
          std::make_shared<RegistrationManager>(configuration.serverHost(), configuration.serverPort(),
                                                factories.redisClientFactory(std::chrono::seconds(0))))
    , mTpmFactory(std::move(factories.tpmFactory))
    , mRuntimeConfiguration(std::make_unique<RuntimeConfiguration>())
// GEMREQ-end A_20974-01
{
    RequestHandlerManager teeHandlers;
    ErpProcessingContext::addPrimaryEndpoints(teeHandlers);
    mTeeServer =
        factories.teeServerFactory(HttpsServer::defaultHost, configuration.serverPort(), std::move(teeHandlers), *this,
                                   false, configuration.getSafeStringValue(ConfigurationKey::SERVER_PROXY_CERTIFICATE));

    auto enrolmentServerPort =
        getEnrolementServerPort(configuration.serverPort(), EnrolmentServer::DefaultEnrolmentServerPort);
    if (enrolmentServerPort)
    {
        RequestHandlerManager enrolmentHandlers;
        EnrolmentServer::addEndpoints(enrolmentHandlers);
        mEnrolmentServer = factories.enrolmentServerFactory(HttpsServer::defaultHost, *enrolmentServerPort,
                                                            std::move(enrolmentHandlers), *this, false, SafeString{});
    }

    RequestHandlerManager adminHandlers;
    AdminServer::addEndpoints(adminHandlers);
    mAdminServer = factories.adminServerFactory(
        configuration.getStringValue(ConfigurationKey::ADMIN_SERVER_INTERFACE),
        gsl::narrow<uint16_t>(configuration.getIntValue(ConfigurationKey::ADMIN_SERVER_PORT)), std::move(adminHandlers),
        *this, false, SafeString{});
    Expect3(mDatabaseFactory != nullptr, "database factory has been passed as nullptr to ServiceContext constructor",
            std::logic_error);
    Expect3(mTpmFactory != nullptr, "mTpmFactory has been passed as nullptr to ServiceContext constructor",
            std::logic_error);
    Expect3(mTslManager != nullptr, "mTslManager could not be initialized", std::logic_error);
}

PcServiceContext::~PcServiceContext()
{
    if (mTslRefreshJob != nullptr)
    {
        mTslRefreshJob->shutdown();
    }
    if (mReportPseudonameKeyRefreshJob != nullptr)
    {
        mReportPseudonameKeyRefreshJob->shutdown();
    }
}

PreUserPseudonymManager& PcServiceContext::getPreUserPseudonymManager()
{
    return *mPreUserPseudonymManager;
}

TelematicPseudonymManager& PcServiceContext::getTelematicPseudonymManager()
{
    return *mTelematicPseudonymManager;
}

const JsonValidator& PcServiceContext::getJsonValidator() const
{
    return *mJsonValidator;
}

std::unique_ptr<Database> PcServiceContext::databaseFactory()
{
    return mDatabaseFactory(*mHsmPool, mKeyDerivation);
}

const RateLimiter& PcServiceContext::getDosHandler()
{
    return *mDosHandler;
}

std::shared_ptr<RedisInterface> PcServiceContext::getRedisClient()
{
    return mRedisClient;
}

HsmPool& PcServiceContext::getHsmPool()
{
    return *mHsmPool;
}

KeyDerivation& PcServiceContext::getKeyDerivation()
{
    return mKeyDerivation;
}

const XmlValidator& PcServiceContext::getXmlValidator() const
{
    return *mXmlValidator;
}

CFdSigErpManager& PcServiceContext::getCFdSigErpManager() const
{
    return *mCFdSigErpManager;
}

Certificate PcServiceContext::getCFdSigErp() const
{
    return mCFdSigErpManager->getCertificate();
}

shared_EVP_PKEY PcServiceContext::getCFdSigErpPrv() const
{
    return mCFdSigErpManager->getPrivateKey();
}

const AuditEventTextTemplates& PcServiceContext::auditEventTextTemplates() const
{
    return mAuditEventTextTemplates;
}

TslManager& PcServiceContext::getTslManager()
{
    return *mTslManager;
}

void PcServiceContext::setPrngSeeder(std::unique_ptr<SeedTimer>&& prngTimer)
{
    mPrngSeeder = std::move(prngTimer);
}

const SeedTimer* PcServiceContext::getPrngSeeder() const
{
    Expect(mPrngSeeder, "mPrngSeeder must not be null");
    return mPrngSeeder.get();
}


ApplicationHealth& PcServiceContext::applicationHealth()
{
    return mApplicationHealth;
}

std::shared_ptr<RegistrationInterface> PcServiceContext::registrationInterface() const
{
    return mRegistrationInterface;
}

const EnrolmentData& PcServiceContext::getEnrolmentData() const
{
    return mEnrolmentData;
}

const Tpm::Factory& PcServiceContext::getTpmFactory() const
{
    return mTpmFactory;
}
HttpsServer& PcServiceContext::getTeeServer() const
{
    return *mTeeServer;
}
HttpsServer* PcServiceContext::getEnrolmentServer() const
{
    return mEnrolmentServer.get();
}
HttpsServer& PcServiceContext::getAdminServer() const
{
    return *mAdminServer;
}

std::shared_ptr<BlobCache> PcServiceContext::getBlobCache() const
{
    return mBlobCache;
}

VsdmKeyBlobDatabase& PcServiceContext::getVsdmKeyBlobDatabase() const
{
    return mVsdmKeyCache->getVsdmKeyBlobDatabase();
}

VsdmKeyCache& PcServiceContext::getVsdmKeyCache() const
{
    return *mVsdmKeyCache;
}

std::shared_ptr<Timer> PcServiceContext::getTimerManager()
{
    return mTimerManager;
}

std::unique_ptr<RuntimeConfigurationGetter> PcServiceContext::getRuntimeConfigurationGetter() const
{
    return std::make_unique<RuntimeConfigurationGetter>(*mRuntimeConfiguration);
}

std::unique_ptr<RuntimeConfigurationSetter> PcServiceContext::getRuntimeConfigurationSetter() const
{
    return std::make_unique<RuntimeConfigurationSetter>(*mRuntimeConfiguration);
}
