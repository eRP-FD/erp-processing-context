/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_PC_PCSERVICECONTEXT_HXX
#define ERP_PROCESSING_CONTEXT_PC_PCSERVICECONTEXT_HXX

#include "erp/database/Database.hxx"
#include "erp/database/redis/RateLimiter.hxx"
#include "erp/pc/CFdSigErpManager.hxx"
#include "erp/pc/pre_user_pseudonym/PreUserPseudonymManager.hxx"
#include "erp/pc/telematic_pseudonym/TelematicPseudonymManager.hxx"
#include "erp/pc/telematik_report_pseudonym/PseudonameKeyRefreshJob.hxx"
#include "erp/server/HttpsServer.hxx"
#include "shared/audit/AuditEventTextTemplates.hxx"
#include "shared/crypto/Certificate.hxx"
#include "shared/idp/Idp.hxx"
#include "shared/server/BaseServiceContext.hxx"
#include "shared/tsl/TslRefreshJob.hxx"
#include "shared/util/Configuration.hxx"

#include <functional>
#include <memory>


class RuntimeConfigurationSetter;
class RuntimeConfigurationGetter;
class BlobCache;
class BlobDatabase;
class VsdmKeyBlobDatabase;
class PreUserPseudonymManager;
class TelematicPseudonymManager;
class JsonValidator;
template<typename>
class PeriodicTimer;
class RegistrationInterface;
class SeedTimerHandler;
class Tpm;
class RequestHandlerManager;
class RegistrationManager;
class RuntimeConfiguration;

using SeedTimer = PeriodicTimer<SeedTimerHandler>;


/**
 * This collection of factories can be extended to allow other classes like database, hsm, etc. to
 * be created differently for production or for tests.
 */
struct Factories : BaseFactories {
    Database::Factory databaseFactory;
    std::function<std::unique_ptr<RedisInterface>(std::chrono::milliseconds socketTimeout)> redisClientFactory;
    HttpsServerFactoryT teeServerFactory;
    std::function<std::shared_ptr<JsonValidator>()> jsonValidatorFactory;
};


/**
 * Service context for requests to the processing client.
 *
 * It MUST NOT contain personal information.
 *
 * Typical members are factories or instances for services like database or HSM access.
 * All functionality must be thread safe.
 */
class PcServiceContext : public BaseServiceContext
{
public:
    PcServiceContext(const Configuration& configuration, Factories&& factories);
    ~PcServiceContext(void) override;

    void setPrngSeeder(std::unique_ptr<SeedTimer>&& prngTimer);

    std::unique_ptr<Database> databaseFactory();
    const RateLimiter& getDosHandler();
    std::shared_ptr<RedisInterface> getRedisClient();
    PreUserPseudonymManager& getPreUserPseudonymManager();
    TelematicPseudonymManager& getTelematicPseudonymManager();
    const JsonValidator& getJsonValidator() const;

    CFdSigErpManager& getCFdSigErpManager() const;
    [[nodiscard]] Certificate getCFdSigErp() const;
    shared_EVP_PKEY getCFdSigErpPrv() const;

    const AuditEventTextTemplates& auditEventTextTemplates() const;

    const SeedTimer* getPrngSeeder() const;

    Idp idp;

    std::shared_ptr<RegistrationInterface> registrationInterface() const;

    BaseHttpsServer& getTeeServer() const;
    BaseHttpsServer& getAdminServer() const;

    std::unique_ptr<RuntimeConfigurationGetter> getRuntimeConfigurationGetter() const;
    std::unique_ptr<RuntimeConfigurationSetter> getRuntimeConfigurationSetter() const;

    PcServiceContext(const PcServiceContext& other) = delete;
    PcServiceContext& operator=(const PcServiceContext& other) = delete;
    PcServiceContext& operator=(PcServiceContext&& other) = delete;

protected:
    /**
     * As database connections to "real" databases are a precious commodity, we are not supposed to hold
     * one for a longer time. Therefore Database objects, which represent (the connection to) an external database
     * are created on demand and destroyed as soon as possible.
     */
    Database::Factory mDatabaseFactory;
    std::shared_ptr<RedisInterface> mRedisClient;
    std::unique_ptr<RateLimiter> mDosHandler;
    const std::shared_ptr<JsonValidator> mJsonValidator;
    std::unique_ptr<PreUserPseudonymManager> mPreUserPseudonymManager;
    std::unique_ptr<TelematicPseudonymManager> mTelematicPseudonymManager;
    std::unique_ptr<CFdSigErpManager> mCFdSigErpManager;
    AuditEventTextTemplates mAuditEventTextTemplates;

    std::unique_ptr<TslRefreshJob> mTslRefreshJob;
    std::unique_ptr<PseudonameKeyRefreshJob> mReportPseudonameKeyRefreshJob;
    std::unique_ptr<SeedTimer> mPrngSeeder;
    std::shared_ptr<RegistrationInterface> mRegistrationInterface;

    std::unique_ptr<BaseHttpsServer> mTeeServer;
    std::unique_ptr<BaseHttpsServer> mAdminServer;

    gsl::not_null<std::unique_ptr<RuntimeConfiguration>> mRuntimeConfiguration;
};

class SessionContext;

using PcSessionContext = SessionContext;

#endif
