/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/server/BaseServiceContext.hxx"
#include "shared/enrolment/EnrolmentServer.hxx"
#include "shared/hsm/VsdmKeyCache.hxx"
#include "shared/util/Configuration.hxx"


// GEMREQ-start A_20974-01
BaseServiceContext::BaseServiceContext(BaseFactories& factories)
    : mTimerManager(std::make_shared<Timer>())
    , mBlobCache(factories.blobCacheFactory())
    , mHsmPool(std::make_unique<HsmPool>(factories.hsmFactoryFactory(factories.hsmClientFactory(), mBlobCache),
                                         factories.teeTokenUpdaterFactory, mTimerManager))
    , mVsdmKeyCache(std::make_unique<VsdmKeyCache>(factories.vsdmKeyBlobDatabaseFactory(), *mHsmPool))
    , mKeyDerivation(*mHsmPool)
    , mXmlValidator(factories.xmlValidatorFactory())
    , mTslManager(factories.tslManagerFactory(mXmlValidator))
    , mTpmFactory(std::move(factories.tpmFactory))
    // GEMREQ-end A_20974-01
{
    Expect3(mTslManager != nullptr, "mTslManager could not be initialized", std::logic_error);
    Expect3(mTpmFactory != nullptr, "mTpmFactory has been passed as nullptr to ServiceContext constructor",
            std::logic_error);
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
