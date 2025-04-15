/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ERPMAIN_HXX
#define ERP_PROCESSING_CONTEXT_ERPMAIN_HXX

#include "shared/MainState.hxx"

#include <functional>
#include <memory>

class ApplicationHealth;
class ApplicationHealthAndRegistrationUpdater;
class BlobCache;
class BlobDatabase;
class DatabaseConnectionTimerHandler;
class HsmClient;
class HsmFactory;
class HsmPool;
class HttpsServer;
class PcServiceContext;
template<typename>
class PeriodicTimer;
class RedisInterface;
class SeedTimerHandler;
class ThreadPool;
class TslManager;
class XmlValidator;

struct Factories;

using SeedTimer = PeriodicTimer<SeedTimerHandler>;
using DatabaseConnectionTimer = PeriodicTimer<DatabaseConnectionTimerHandler>;

/**
 * A collection of high-level functions that are required to initialize and run the processing context application.
 * Exposed in a form that can also be used by tests.
 * Only runApplication() has to be run explicitly. It will call the other functions.
 */
class ErpMain
{
public:
    static Factories createProductionFactories();

    static int runApplication (
        Factories&& factories,
        MainStateCondition& state,
        const std::function<void(PcServiceContext&)>& postInitializationCallback = {});

private:
    static std::unique_ptr<ApplicationHealthAndRegistrationUpdater> setupHeartbeatSender(PcServiceContext& serviceContext);

    static std::unique_ptr<SeedTimer> setupPrngSeeding (
        ThreadPool& threadpool,
        HsmPool& randomSource);

    static std::unique_ptr<DatabaseConnectionTimer> setupDatabaseTimer(PcServiceContext& serviceContext);

    static bool waitForHealthUp (PcServiceContext& serviceContext);


};


#endif
