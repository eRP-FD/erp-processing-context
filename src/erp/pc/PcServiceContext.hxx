/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_PC_PCSERVICECONTEXT_HXX
#define ERP_PROCESSING_CONTEXT_PC_PCSERVICECONTEXT_HXX

#include "erp/crypto/Certificate.hxx"
#include "erp/database/Database.hxx"
#include "erp/database/redis/RateLimiter.hxx"
#include "erp/enrolment/EnrolmentData.hxx"
#include "erp/hsm/BlobCache.hxx"
#include "erp/hsm/HsmFactory.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/hsm/KeyDerivation.hxx"
#include "erp/idp/Idp.hxx"
#include "erp/pc/CFdSigErpManager.hxx"
#include "erp/pc/pre_user_pseudonym/PreUserPseudonymManager.hxx"
#include "erp/pc/telematic_pseudonym/TelematicPseudonymManager.hxx"
#include "erp/pc/telematik_report_pseudonym/PseudonameKeyRefreshJob.hxx"
#include "erp/server/HttpsServer.hxx"
#include "erp/service/AuditEventTextTemplates.hxx"
#include "erp/tpm/Tpm.hxx"
#include "erp/tsl/TslManager.hxx"
#include "erp/tsl/TslRefreshJob.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/health/ApplicationHealth.hxx"

#include <functional>
#include <memory>
#include <mutex>


class RuntimeConfigurationSetter;
class RuntimeConfigurationGetter;
class BlobCache;
class BlobDatabase;
class PreUserPseudonymManager;
class TelematicPseudonymManager;
class JsonValidator;
class XmlValidator;
template<typename>
class PeriodicTimer;
class RegistrationInterface;
class SeedTimerHandler;
class Tpm;
class RequestHandlerManager;
class RegistrationManager;
class VsdmKeyBlobDatabase;
class VsdmKeyCache;
class RuntimeConfiguration;

using SeedTimer = PeriodicTimer<SeedTimerHandler>;

/**
 * This collection of factories can be extended to allow other classes like database, hsm, etc. to
 * be created differently for production or for tests.
 */
struct Factories {
    Database::Factory databaseFactory;

    std::function<std::shared_ptr<BlobCache>()> blobCacheFactory;
    std::function<std::unique_ptr<HsmClient>()> hsmClientFactory;
    std::function<std::unique_ptr<HsmFactory>(std::unique_ptr<HsmClient>, std::shared_ptr<BlobCache>)>
        hsmFactoryFactory;
    TeeTokenUpdater::TeeTokenUpdaterFactory teeTokenUpdaterFactory;
    TslManager::TslManagerFactory tslManagerFactory;
    std::function<std::unique_ptr<RedisInterface>(std::chrono::milliseconds socketTimeout)> redisClientFactory;
    std::function<std::unique_ptr<VsdmKeyBlobDatabase>()> vsdmKeyBlobDatabaseFactory;

    using HttpsServerFactoryT = std::function<std::unique_ptr<HttpsServer>(
        const std::string_view address, uint16_t port, RequestHandlerManager&& requestHandlers,
        PcServiceContext& serviceContext, bool enforceClientAuthentication, const SafeString& caCertificates)>;

    HttpsServerFactoryT teeServerFactory;
    HttpsServerFactoryT enrolmentServerFactory;
    HttpsServerFactoryT adminServerFactory;

    Tpm::Factory tpmFactory;

    std::function<std::shared_ptr<JsonValidator>()> jsonValidatorFactory;
    std::function<std::shared_ptr<XmlValidator>()> xmlValidatorFactory;
};


/**
 * Service context for requests to the processing client.
 *
 * It MUST NOT contain personal information.
 *
 * Typical members are factories or instances for services like database or HSM access.
 * All functionality must be thread safe.
 */
class PcServiceContext
{
public:
    PcServiceContext(const Configuration& configuration, Factories&& factories);
    ~PcServiceContext(void);

    void setPrngSeeder(std::unique_ptr<SeedTimer>&& prngTimer);

    std::unique_ptr<Database> databaseFactory();
    const RateLimiter& getDosHandler();
    std::shared_ptr<RedisInterface> getRedisClient();
    HsmPool& getHsmPool();
    KeyDerivation& getKeyDerivation();
    PreUserPseudonymManager& getPreUserPseudonymManager();
    TelematicPseudonymManager& getTelematicPseudonymManager();
    const JsonValidator& getJsonValidator() const;
    const XmlValidator& getXmlValidator() const;

    CFdSigErpManager& getCFdSigErpManager() const;
    [[nodiscard]] Certificate getCFdSigErp() const;
    shared_EVP_PKEY getCFdSigErpPrv() const;

    const AuditEventTextTemplates& auditEventTextTemplates() const;

    /**
     * The ownership over the object stays by PcServiceContext ( and its refresh job ).
     */
    TslManager& getTslManager();

    const SeedTimer* getPrngSeeder() const;

    Idp idp;
    ApplicationHealth& applicationHealth();

    std::shared_ptr<RegistrationInterface> registrationInterface() const;

    const EnrolmentData& getEnrolmentData() const;
    const Tpm::Factory& getTpmFactory() const;

    HttpsServer& getTeeServer() const;
    HttpsServer* getEnrolmentServer() const;
    HttpsServer& getAdminServer() const;

    std::shared_ptr<BlobCache> getBlobCache() const;
    VsdmKeyBlobDatabase& getVsdmKeyBlobDatabase() const;
    VsdmKeyCache& getVsdmKeyCache() const;

    std::shared_ptr<Timer> getTimerManager();

    std::unique_ptr<RuntimeConfigurationGetter> getRuntimeConfigurationGetter() const;
    std::unique_ptr<RuntimeConfigurationSetter> getRuntimeConfigurationSetter() const;

    PcServiceContext(const PcServiceContext& other) = delete;
    PcServiceContext& operator=(const PcServiceContext& other) = delete;
    PcServiceContext& operator=(PcServiceContext&& other) = delete;

private:
    std::shared_ptr<Timer> mTimerManager;
    /**
     * As database connections to "real" databases are a precious commodity, we are not supposed to hold
     * one for a longer time. Therefore Database objects, which represent (the connection to) an external database
     * are created on demand and destroyed as soon as possible.
     */
    Database::Factory mDatabaseFactory;
    std::shared_ptr<RedisInterface> mRedisClient;
    std::unique_ptr<RateLimiter> mDosHandler;
    std::shared_ptr<BlobCache> mBlobCache;
    std::unique_ptr<HsmPool> mHsmPool;
    std::unique_ptr<VsdmKeyCache> mVsdmKeyCache;
    KeyDerivation mKeyDerivation;
    const std::shared_ptr<JsonValidator> mJsonValidator;
    const std::shared_ptr<XmlValidator> mXmlValidator;
    std::unique_ptr<PreUserPseudonymManager> mPreUserPseudonymManager;
    std::unique_ptr<TelematicPseudonymManager> mTelematicPseudonymManager;
    /**
     * The manager can be used to get actual TSL-/BNetzA-VL related information.
     * The refresh-job takes care to hold the manager actual.
     */
    std::shared_ptr<TslManager> mTslManager;
    std::unique_ptr<CFdSigErpManager> mCFdSigErpManager;
    AuditEventTextTemplates mAuditEventTextTemplates;

    std::unique_ptr<TslRefreshJob> mTslRefreshJob;
    std::unique_ptr<PseudonameKeyRefreshJob> mReportPseudonameKeyRefreshJob;
    std::unique_ptr<SeedTimer> mPrngSeeder;
    ApplicationHealth mApplicationHealth;
    std::shared_ptr<RegistrationInterface> mRegistrationInterface;

    EnrolmentData mEnrolmentData;
    Tpm::Factory mTpmFactory;

    std::unique_ptr<HttpsServer> mTeeServer;
    std::unique_ptr<HttpsServer> mEnrolmentServer;
    std::unique_ptr<HttpsServer> mAdminServer;

    gsl::not_null<std::unique_ptr<RuntimeConfiguration>> mRuntimeConfiguration;
};

class SessionContext;

using PcSessionContext = SessionContext;

#endif
