/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_ERPMAIN_HXX
#define ERP_PROCESSING_CONTEXT_ERPMAIN_HXX

#include "erp/database/Database.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/registration/HeartbeatSender.hxx"
#include "erp/tsl/TslManager.hxx"
#include "erp/tsl/TslRefreshJob.hxx"
#include "erp/util/Condition.hxx"
#include "erp/util/Configuration.hxx"

#include <memory>
#include <optional>

class AdminServer;
class ApplicationHealth;
class BlobCache;
class BlobDatabase;
class EnrolmentServer;
class ErpProcessingContext;
class HsmClient;
class HsmFactory;
class PcServiceContext;
class RedisInterface;
class SeedTimer;
class ThreadPool;

/**
 * A collection of high-level functions that are required to initialize and run the processing context application.
 * Exposed in a form that can also be used by tests.
 * Only runApplication() has to be run explicitly. It will call the other functions.
 */
class ErpMain
{
public:
    /**
     * This collection of factories can be extended to allow other classes like database, hsm, etc. to
     * be created differently for production or for tests.
     */
    struct Factories
    {
        Database::Factory databaseFactory;

        std::function<std::shared_ptr<BlobCache>()> blobCacheFactory;
        std::function<std::unique_ptr<HsmClient>()> hsmClientFactory;
        std::function<std::unique_ptr<HsmFactory>(std::unique_ptr<HsmClient>,std::shared_ptr<BlobCache>)> hsmFactoryFactory;
        HsmPool::TeeTokenUpdaterFactory teeTokenUpdaterFactory;
        TslManager::TslManagerFactory tslManagerFactory;
        std::function<std::unique_ptr<RedisInterface>()> redisClientFactory;
    };

    static Factories createProductionFactories();

    /**
     * This set of states is primarily intended for tests so that a test can wait for a certain state to be
     * reached. See ErpMainTest.cxx for an example.
     */
    enum class State
    {
        Unknown,
        Initializing,
        WaitingForTermination,
        Terminating,
        Terminated
    };
    using StateCondition = Condition<State>;

    static int runApplication (
        uint16_t defaultEnrolmentServerPort,
        Factories&& factories,
        StateCondition& state,
        std::function<void(PcServiceContext&)> postInitializationCallback = {});

    static std::unique_ptr<HeartbeatSender> setupHeartbeatSender(PcServiceContext& serviceContext);

    static std::unique_ptr<SeedTimer> setupPrngSeeding (
        ThreadPool& threadpool,
        size_t threadCount,
        HsmPool& randomSource);

    static std::unique_ptr<ErpProcessingContext> setupTeeServer (
        const uint16_t port,
        std::unique_ptr<HsmFactory> hsmFactory,
        Database::Factory&& database,
        HsmPool::TeeTokenUpdaterFactory&& teeTokenUpdaterFactory,
        TslManager::TslManagerFactory&& tslManagerFactory,
        std::function<std::unique_ptr<RedisInterface>()>&& redisClientFactory);

    static std::unique_ptr<EnrolmentServer> setupEnrolmentServer (
        const uint16_t enrolmentPort,
        const std::shared_ptr<BlobCache>& blobCache,
        const std::shared_ptr<TslManager>& tslManager,
        const std::shared_ptr<HsmPool>& hsmPool);

    static std::unique_ptr<AdminServer> setupAdminServer(const std::string_view& interface, const uint16_t port);

    static std::shared_ptr<TslManager> setupTslManager (const std::shared_ptr<XmlValidator>& xmlValidator);

    /**
     * Return the optional port for the enrolment service.
     * If the enrolment service is not active in the current configuration then the returned optional remains empty.
     */
    static std::optional<uint16_t> getEnrolementServerPort (
        const uint16_t pcPort,
        const uint16_t defaultEnrolmentServerPort);

    static bool waitForHealthUp (ErpProcessingContext& serviceContext);


};


#endif
