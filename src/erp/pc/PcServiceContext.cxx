/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/pc/PcServiceContext.hxx"


#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/database/Database.hxx"
#include "erp/pc/SeedTimer.hxx"
#include "erp/service/DosHandler.hxx"
#include "erp/validation/JsonValidator.hxx"
#include "erp/registration/RegistrationInterface.hxx"
#include "erp/ErpProcessingContext.hxx"
#include "erp/enrolment/EnrolmentServer.hxx"
#include "erp/admin/AdminServer.hxx"
#include "erp/registration/RegistrationManager.hxx"
#include "erp/ErpRequirements.hxx"


namespace
{
    std::unique_ptr<TslRefreshJob> setupTslRefreshJob(
        TslManager& tslManager,
        const Configuration& configuration)
    {
        GS_A_4899.start("Create asynchronous TSL-Update job.");
        const std::chrono::seconds tslRefreshInterval{
            configuration.getOptionalIntValue(
                ConfigurationKey::TSL_REFRESH_INTERVAL, 86400)}; // 24 Hours per default
        auto refreshJob = std::make_unique<TslRefreshJob>(tslManager, tslRefreshInterval);
        refreshJob->start();
        GS_A_4899.finish();
        return refreshJob;
    }
}


// GEMREQ-start A_20974-01
PcServiceContext::PcServiceContext(const Configuration& configuration, Factories&& factories)
    : idp()
    , mTimerManager(std::make_shared<Timer>())
    , mDatabaseFactory(std::move(factories.databaseFactory))
    , mRedisClient(factories.redisClientFactory())
    , mDosHandler(std::make_unique<DosHandler>(mRedisClient))
    , mBlobCache(factories.blobCacheFactory())
    , mHsmPool(std::make_unique<HsmPool>(factories.hsmFactoryFactory(factories.hsmClientFactory(), mBlobCache),
                                        factories.teeTokenUpdaterFactory, mTimerManager))
    , mKeyDerivation(*mHsmPool)
    , mJsonValidator(factories.jsonValidatorFactory())
    , mXmlValidator(factories.xmlValidatorFactory())
    , mInCodeValidator(factories.incodeValidatorFactory())
    , mPreUserPseudonymManager(PreUserPseudonymManager::create(this))
    , mTelematicPseudonymManager(TelematicPseudonymManager::create(this))
    , mTslManager(factories.tslManagerFactory(mXmlValidator))
    , mCFdSigErpManager(std::make_unique<CFdSigErpManager>(configuration, *mTslManager, *mHsmPool))
    , mTslRefreshJob(setupTslRefreshJob(*mTslManager, configuration))
    , mReportPseudonameKeyRefreshJob(PseudonameKeyRefreshJob::setupPseudonameKeyRefreshJob(*mHsmPool, getBlobCache(), configuration))
    , mRegistrationInterface(std::make_shared<RegistrationManager>(configuration.serverHost(), configuration.serverPort(), factories.redisClientFactory()))
    , mTpmFactory(std::move(factories.tpmFactory))
// GEMREQ-end A_20974-01
{
    RequestHandlerManager teeHandlers;
    ErpProcessingContext::addPrimaryEndpoints(teeHandlers);
    mTeeServer = factories.teeServerFactory(
        HttpsServer::defaultHost, configuration.serverPort(), std::move(teeHandlers), *this, false,
        SafeString(configuration.getOptionalStringValue(ConfigurationKey::SERVER_PROXY_CERTIFICATE, "")));

    auto enrolmentServerPort =
        getEnrolementServerPort(configuration.serverPort(), EnrolmentServer::DefaultEnrolmentServerPort);
    if (enrolmentServerPort)
    {
        RequestHandlerManager enrolmentHandlers;
        EnrolmentServer::addEndpoints(enrolmentHandlers);
        mEnrolmentServer =
            factories.enrolmentServerFactory(HttpsServer::defaultHost, *enrolmentServerPort,
                                             std::move(enrolmentHandlers), *this, false, SafeString{});
    }

    RequestHandlerManager adminHandlers;
    AdminServer::addEndpoints(adminHandlers);
    mAdminServer = factories.adminServerFactory(
        configuration.getStringValue(ConfigurationKey::ADMIN_SERVER_INTERFACE),
        gsl::narrow<uint16_t>(configuration.getIntValue(ConfigurationKey::ADMIN_SERVER_PORT)), std::move(adminHandlers),
        *this, false, SafeString{});
    Expect3(mDatabaseFactory!=nullptr, "database factory has been passed as nullptr to ServiceContext constructor", std::logic_error);
    Expect3(mTpmFactory!=nullptr, "mTpmFactory has been passed as nullptr to ServiceContext constructor", std::logic_error);
    Expect3(mTslManager!=nullptr, "mTslManager could not be initialized", std::logic_error);
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

const DosHandler& PcServiceContext::getDosHandler()
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

KeyDerivation & PcServiceContext::getKeyDerivation()
{
    return mKeyDerivation;
}

const XmlValidator& PcServiceContext::getXmlValidator() const
{
    return *mXmlValidator;
}

const InCodeValidator& PcServiceContext::getInCodeValidator() const
{
    return *mInCodeValidator;
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


ApplicationHealth& PcServiceContext::applicationHealth ()
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
BlobCache& PcServiceContext::getBlobCache() const
{
    return *mBlobCache;
}
std::shared_ptr<Timer> PcServiceContext::getTimerManager()
{
    return mTimerManager;
}
