/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/pc/PcServiceContext.hxx"


#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/database/Database.hxx"
#include "erp/service/DosHandler.hxx"
#include "erp/validation/JsonValidator.hxx"
#include "erp/registration/RegistrationInterface.hxx"

#include "erp/ErpRequirements.hxx"


namespace
{
    std::unique_ptr<TslRefreshJob> setupTslRefreshJob(
        const std::shared_ptr<TslManager>& tslManager,
        const Configuration& configuration)
    {
        std::unique_ptr<TslRefreshJob> refreshJob;
        if (tslManager != nullptr)
        {
            GS_A_4899.start("Create asynchronous TSL-Update job.");
            std::chrono::seconds tslRefreshInterval{
                configuration.getOptionalIntValue(
                    ConfigurationKey::TSL_REFRESH_INTERVAL, 86400)}; // 24 Hours per default
            refreshJob = std::make_unique<TslRefreshJob>(tslManager, tslRefreshInterval);
            refreshJob->start();
            GS_A_4899.finish();
        }

        return refreshJob;
    }
}


PcServiceContext::PcServiceContext(const Configuration& configuration,
                                   Database::Factory&& databaseFactory,
                                   std::unique_ptr<RedisInterface>&& redisClient,
                                   std::unique_ptr<HsmPool>&& hsmPool,
                                   std::shared_ptr<JsonValidator> jsonValidator,
                                   std::shared_ptr<XmlValidator> xmlValidator,
                                   std::shared_ptr<InCodeValidator> inCodeValidator,
                                   std::unique_ptr<RegistrationInterface> registrationInterface,
                                   std::shared_ptr<TslManager> tslManager)
    : idp()
    , mDatabaseFactory(std::move(databaseFactory))
    , mRedisClient(std::move(redisClient))
    , mDosHandler(std::make_unique<DosHandler>(mRedisClient))
    , mHsmPool(std::move(hsmPool))
    , mKeyDerivation(*mHsmPool)
    , mJsonValidator(std::move(jsonValidator))
    , mXmlValidator(std::move(xmlValidator))
    , mInCodeValidator(inCodeValidator)
    , mPreUserPseudonymManager(PreUserPseudonymManager::create(this))
    , mTelematicPseudonymManager(TelematicPseudonymManager::create(*this))
    , mCFdSigErpManager(std::make_unique<CFdSigErpManager>(configuration, tslManager))
    , mCFdSigErpPrivateKey()
    , mTslManager(std::move(tslManager))
    , mTslRefreshJob(setupTslRefreshJob(mTslManager, configuration))
    , mRegistrationInterface(std::move(registrationInterface))
{
    Expect3(mDatabaseFactory!=nullptr, "database factory has been passed as nullptr to ServiceContext constructor", std::logic_error);
    Expect3(mJsonValidator!=nullptr, "jsonValidator has been passed as nullptr to ServiceContext constructor", std::logic_error);
    Expect3(mXmlValidator!=nullptr, "xmlValidator has been passed as nullptr to ServiceContext constructor", std::logic_error);
}

PcServiceContext::~PcServiceContext()
{
    if (mTslRefreshJob != nullptr)
    {
        mTslRefreshJob->shutdown();
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

const Certificate& PcServiceContext::getCFdSigErp() const
{
    return mCFdSigErpManager->getCertificate();
}

const shared_EVP_PKEY& PcServiceContext::getCFdSigErpPrv() const
{
    std::lock_guard lock (mMutex);
    if ( ! mCFdSigErpPrivateKey.isSet())
        mCFdSigErpPrivateKey = mHsmPool->acquire().session().getVauSigPrivateKey();
    return mCFdSigErpPrivateKey;
}

const AuditEventTextTemplates& PcServiceContext::auditEventTextTemplates() const
{
    return mAuditEventTextTemplates;
}

TslManager* PcServiceContext::getTslManager()
{
    return mTslManager.get();
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
