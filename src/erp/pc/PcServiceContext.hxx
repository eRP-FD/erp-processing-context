#ifndef ERP_PROCESSING_CONTEXT_PC_PCSERVICECONTEXT_HXX
#define ERP_PROCESSING_CONTEXT_PC_PCSERVICECONTEXT_HXX

#include "erp/database/Database.hxx"
#include "erp/hsm/HsmFactory.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/hsm/KeyDerivation.hxx"
#include "erp/idp/Idp.hxx"
#include "erp/pc/SeedTimer.hxx"
#include "erp/pc/pre_user_pseudonym/PreUserPseudonymManager.hxx"
#include "erp/service/AuditEventTextTemplates.hxx"
#include "erp/service/DosHandler.hxx"
#include "erp/tsl/TslManager.hxx"
#include "erp/tsl/TslRefreshJob.hxx"
#include "erp/util/Configuration.hxx"

#include <erp/crypto/Certificate.hxx>
#include <functional>
#include <memory>
#include <mutex>


class BlobCache;
class PreUserPseudonymManager;
class JsonValidator;
class XmlValidator;
class SeedTimer;


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
    PcServiceContext(const Configuration& configuration,
                   Database::Factory&& databaseFactory,
                   std::unique_ptr<DosHandler>&& dosHandler,
                   std::unique_ptr<HsmPool>&& hsmPool,
                   std::shared_ptr<JsonValidator> jsonValidator,
                   std::shared_ptr<XmlValidator> xmlValidator,
                   std::shared_ptr<TslManager> tslManager = {});
    ~PcServiceContext(void);

    void setPrngSeeder(std::unique_ptr<SeedTimer>&& prngTimer);

    std::unique_ptr<Database> databaseFactory();
    const DosHandler& getDosHandler();
    HsmPool& getHsmPool();
    KeyDerivation& getKeyDerivation();
    PreUserPseudonymManager& getPreUserPseudonymManager();
    const JsonValidator& getJsonValidator() const;
    const XmlValidator& getXmlValidator() const;

    const Certificate& getCFdSigErp() const;
    const shared_EVP_PKEY& getCFdSigErpPrv() const;

    const AuditEventTextTemplates& auditEventTextTemplates() const;

    /**
     * The ownership over the object stays by PcServiceContext ( and its refresh job ).
     */
    TslManager* getTslManager();

    const SeedTimer* getPrngSeeder() const;

    Idp idp;

    PcServiceContext(const PcServiceContext& other) = delete;
    PcServiceContext& operator=(const PcServiceContext& other) = delete;
    PcServiceContext& operator=(PcServiceContext&& other) = delete;

private:
    mutable std::mutex mMutex;
    /**
     * As database connections to "real" databases are a precious commodity, we are not supposed to hold
     * one for a longer time. Therefore Database objects, which represent (the connection to) an external database
     * are created on demand and destroyed as soon as possible.
     */
    Database::Factory mDatabaseFactory;
    std::unique_ptr<DosHandler> mDosHandler;
    std::unique_ptr<HsmPool> mHsmPool;
    KeyDerivation mKeyDerivation;
    const std::shared_ptr<JsonValidator> mJsonValidator;
    const std::shared_ptr<XmlValidator> mXmlValidator;
    std::unique_ptr<PreUserPseudonymManager> mPreUserPseudonymManager;
    Certificate mCFdSigErp;
    mutable shared_EVP_PKEY mCFdSigErpPrivateKey;
    AuditEventTextTemplates mAuditEventTextTemplates;

    /**
     * The manager can be used to get actual TSL-/BNetzA-VL related information.
     * The refresh-job takes care to hold the manager actual.
     */
    std::shared_ptr<TslManager> mTslManager;
    std::unique_ptr<TslRefreshJob> mTslRefreshJob;
    std::unique_ptr<SeedTimer> mPrngSeeder;
};

template<class ServiceContextType> class SessionContext;

using PcSessionContext = SessionContext<PcServiceContext>;

#endif
