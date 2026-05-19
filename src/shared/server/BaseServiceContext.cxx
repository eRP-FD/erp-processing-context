/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/server/BaseServiceContext.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/enrolment/EnrolmentServer.hxx"
#include "shared/hsm/VsdmKeyCache.hxx"
#include "shared/tsl/TslRefreshJob.hxx"
#include "shared/util/Configuration.hxx"


// GEMREQ-start A_20974-01
BaseServiceContext::BaseServiceContext(const BaseFactories& factories)
    : mTimerManager(std::make_shared<Timer>())
    , mBlobCache(factories.blobCacheFactory())
    , mHsmPool(std::make_unique<HsmPool>(factories.hsmFactoryFactory(factories.hsmClientFactory(), mBlobCache),
                                         factories.teeTokenUpdaterFactory, mTimerManager))
    , mVsdmKeyCache(std::make_unique<VsdmKeyCache>(factories.vsdmKeyBlobDatabaseFactory(), *mHsmPool))
    , mKeyDerivation(*mHsmPool)
    , mXmlValidator(factories.xmlValidatorFactory())
    , mTpmFactory(factories.tpmFactory)
    , mTslManager(factories.tslManagerFactory(mXmlValidator))
    // GEMREQ-end A_20974-01
{
    Expect3(mTslManager != nullptr, "mTslManager could not be initialized", std::logic_error);
    Expect3(mTpmFactory != nullptr, "mTpmFactory has been passed as nullptr to ServiceContext constructor",
            std::logic_error);
}

BaseServiceContext::~BaseServiceContext()
{
    if (mTslRefreshJob != nullptr)
    {
        mTslRefreshJob->shutdown();
    }
}

std::shared_ptr<Timer> BaseServiceContext::getTimerManager()
{
    return mTimerManager;
}

const XmlValidator& BaseServiceContext::getXmlValidator() const
{
    return *mXmlValidator;
}

std::shared_ptr<BlobCache> BaseServiceContext::getBlobCache() const
{
    return mBlobCache;
}

TslManager& BaseServiceContext::getTslManager()
{
    return *mTslManager;
}

const Tpm::Factory& BaseServiceContext::getTpmFactory() const
{
    return mTpmFactory;
}

BaseHttpsServer& BaseServiceContext::getAdminServer() const
{
    return *mAdminServer;
}

BaseHttpsServer* BaseServiceContext::getEnrolmentServer() const
{
    return mEnrolmentServer.get();
}

VsdmKeyCache& BaseServiceContext::getVsdmKeyCache() const
{
    return *mVsdmKeyCache;
}

const EnrolmentData& BaseServiceContext::getEnrolmentData() const
{
    return mEnrolmentData;
}

VsdmKeyBlobDatabase& BaseServiceContext::getVsdmKeyBlobDatabase() const
{
    return mVsdmKeyCache->getVsdmKeyBlobDatabase();
}

HsmPool& BaseServiceContext::getHsmPool()
{
    return *mHsmPool;
}

KeyDerivation& BaseServiceContext::getKeyDerivation()
{
    return mKeyDerivation;
}

ApplicationHealth& BaseServiceContext::applicationHealth()
{
    return mApplicationHealth;
}

void BaseServiceContext::setupTslRefreshJob(std::chrono::seconds tslRefreshInterval)
{
    GS_A_4899.start("Create asynchronous TSL-Update job.");
    mTslRefreshJob = std::make_unique<TslRefreshJob>(*mTslManager, tslRefreshInterval);
    mTslRefreshJob->start();
    GS_A_4899.finish();
}

std::shared_ptr<CrlProvider> BaseServiceContext::crlProvider()
{
    return mCrlProvider;
}
