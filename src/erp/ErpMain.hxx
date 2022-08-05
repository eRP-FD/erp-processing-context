/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_ERPMAIN_HXX
#define ERP_PROCESSING_CONTEXT_ERPMAIN_HXX

#include "erp/database/Database.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/registration/ApplicationHealthAndRegistrationUpdater.hxx"
#include "erp/tsl/TslManager.hxx"
#include "erp/tsl/TslRefreshJob.hxx"
#include "erp/util/Condition.hxx"
#include "erp/util/Configuration.hxx"

#include <memory>
#include <optional>

class ApplicationHealth;
class BlobCache;
class BlobDatabase;
class HsmClient;
class HsmFactory;
class PcServiceContext;
class RedisInterface;
class SeedTimer;
class ThreadPool;
struct Factories;
class HttpsServer;

/**
 * A collection of high-level functions that are required to initialize and run the processing context application.
 * Exposed in a form that can also be used by tests.
 * Only runApplication() has to be run explicitly. It will call the other functions.
 */
class ErpMain
{
public:


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
        Factories&& factories,
        StateCondition& state,
        const std::function<void(PcServiceContext&)>& postInitializationCallback = {});

private:
    static std::unique_ptr<ApplicationHealthAndRegistrationUpdater> setupHeartbeatSender(PcServiceContext& serviceContext);

    static std::unique_ptr<SeedTimer> setupPrngSeeding (
        ThreadPool& threadpool,
        HsmPool& randomSource);

    static std::shared_ptr<TslManager> setupTslManager (const std::shared_ptr<XmlValidator>& xmlValidator);

    static bool waitForHealthUp (PcServiceContext& serviceContext);


};


#endif
