/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SHARED_BASESERVICECONTEXT_HXX
#define ERP_PROCESSING_CONTEXT_SHARED_BASESERVICECONTEXT_HXX

#include "shared/enrolment/EnrolmentData.hxx"
#include "shared/hsm/BlobCache.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/hsm/KeyDerivation.hxx"
#include "shared/hsm/TeeTokenUpdater.hxx"
#include "shared/hsm/VsdmKeyCache.hxx"
#include "shared/server/BaseHttpsServer.hxx"
#include "shared/tpm/Tpm.hxx"
#include "shared/tsl/TslManager.hxx"
#include "shared/util/health/ApplicationHealth.hxx"


class BaseServiceContext;
class RequestHandlerManager;
class VsdmKeyBlobDatabase;
class XmlValidator;


/**
 * This collection of factories can be extended to allow other classes like database, hsm, etc. to
 * be created differently for production or for tests.
 */
struct BaseFactories {
    std::function<std::shared_ptr<BlobCache>()> blobCacheFactory;
    std::function<std::unique_ptr<HsmClient>()> hsmClientFactory;
    TeeTokenUpdater::TeeTokenUpdaterFactory teeTokenUpdaterFactory;
    std::function<std::unique_ptr<HsmFactory>(std::unique_ptr<HsmClient>, std::shared_ptr<BlobCache>)>
        hsmFactoryFactory;
    TslManager::TslManagerFactory tslManagerFactory;
    std::function<std::unique_ptr<VsdmKeyBlobDatabase>()> vsdmKeyBlobDatabaseFactory;
    using HttpsServerFactoryT = std::function<std::unique_ptr<BaseHttpsServer>(
        const std::string_view address, uint16_t port, RequestHandlerManager&& requestHandlers,
        BaseServiceContext& serviceContext, bool enforceClientAuthentication, const SafeString& caCertificates)>;
    HttpsServerFactoryT enrolmentServerFactory;
    Tpm::Factory tpmFactory;
    std::function<std::shared_ptr<XmlValidator>()> xmlValidatorFactory;
    HttpsServerFactoryT adminServerFactory;
};


class BaseServiceContext
{
public:
    BaseServiceContext(const BaseFactories& factories);
    virtual ~BaseServiceContext() = default;

    std::shared_ptr<Timer> getTimerManager();

    const XmlValidator& getXmlValidator() const;

    std::shared_ptr<BlobCache> getBlobCache() const;

    /**
     * The ownership over the object stays by PcServiceContext ( and its refresh job ).
     */
    TslManager& getTslManager();

    const Tpm::Factory& getTpmFactory() const;

    BaseHttpsServer& getAdminServer() const;

    BaseHttpsServer* getEnrolmentServer() const;

    VsdmKeyCache& getVsdmKeyCache() const;

    const EnrolmentData& getEnrolmentData() const;

    HsmPool& getHsmPool();

    // Shortcut to getVsdmKeyCache()->getVsdmKeyBlobDatabase()
    VsdmKeyBlobDatabase& getVsdmKeyBlobDatabase() const;

    KeyDerivation& getKeyDerivation();

    ApplicationHealth& applicationHealth();

protected:
    std::shared_ptr<Timer> mTimerManager;
    std::shared_ptr<BlobCache> mBlobCache;
    std::unique_ptr<HsmPool> mHsmPool;
    std::unique_ptr<VsdmKeyCache> mVsdmKeyCache;
    KeyDerivation mKeyDerivation;
    const std::shared_ptr<XmlValidator> mXmlValidator;
    /**
     * The manager can be used to get actual TSL-/BNetzA-VL related information.
     * The refresh-job takes care to hold the manager actual.
     */
    std::shared_ptr<TslManager> mTslManager;
    Tpm::Factory mTpmFactory;
    std::unique_ptr<BaseHttpsServer> mEnrolmentServer;
    std::unique_ptr<BaseHttpsServer> mAdminServer;
    EnrolmentData mEnrolmentData;
    ApplicationHealth mApplicationHealth;
};


#endif
