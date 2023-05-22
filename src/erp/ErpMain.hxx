/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ERPMAIN_HXX
#define ERP_PROCESSING_CONTEXT_ERPMAIN_HXX

#include <memory>
#include <functional>

class ApplicationHealth;
class ApplicationHealthAndRegistrationUpdater;
class BlobCache;
class BlobDatabase;
template <typename>
class Condition;
class HsmClient;
class HsmFactory;
class HsmPool;
class PcServiceContext;
template<typename>
class PeriodicTimer;
class RedisInterface;
class SeedTimerHandler;
class TslManager;
class ThreadPool;
struct Factories;
class HttpsServer;
class XmlValidator;

using SeedTimer = PeriodicTimer<SeedTimerHandler>;

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
