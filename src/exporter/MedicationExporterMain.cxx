/*
* (C) Copyright IBM Deutschland GmbH 2021, 2024
* (C) Copyright IBM Corp. 2021, 2024
*
* non-exclusively licensed to gematik GmbH
*/

#include "exporter/MedicationExporterMain.hxx"
#include "exporter/EventProcessor.hxx"
#include "exporter/client/EpaMedicationClientImpl.hxx"
#include "exporter/network/client/Tee3ClientPool.hxx"
#include "exporter/database/MedicationExporterDatabaseFrontend.hxx"
#include "exporter/database/MedicationExporterPostgresBackend.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "exporter/server/HttpsServer.hxx"
#include "shared/deprecated/SignalHandler.hxx"
#include "shared/deprecated/TerminationHandler.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/hsm/production/ProductionBlobDatabase.hxx"
#include "shared/hsm/production/ProductionVsdmKeyBlobDatabase.hxx"
#include "shared/util/Configuration.hxx"

#if WITH_HSM_MOCK > 0
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/tpm/TpmMock.hxx"
#include "mock/tsl/MockTslManager.hxx"
#endif
#if WITH_HSM_TPM_PRODUCTION > 0
#include "shared/hsm/production/HsmProductionClient.hxx"
#include "shared/hsm/production/HsmProductionFactory.hxx"
#include "shared/tpm/TpmProduction.hxx"
#endif

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>

boost::asio::awaitable<void> setupEpaClientPool(MedicationExporterServiceContext& serviceContext)
{
    auto fqdns =
            Configuration::instance().getArray(ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_EPA_AS_FQDN);
    std::vector<std::tuple<std::string, uint16_t>> hostPortList;
    for (const auto& fqdn : fqdns)
    {
        auto urlParts = UrlHelper::parseUrl(fqdn);
        co_await serviceContext.teeClientPool()->addEpaHost(urlParts.mHost, gsl::narrow<uint16_t>(urlParts.mPort));
    }

}


MedicationExporterFactories MedicationExporterMain::createProductionFactories()
{
    MedicationExporterFactories factories;

    factories.blobCacheFactory = [] {
        return std::make_shared<BlobCache>(std::make_unique<ProductionBlobDatabase>());
    };

    factories.vsdmKeyBlobDatabaseFactory = []() {
        return std::make_unique<ProductionVsdmKeyBlobDatabase>();
    };

    factories.tslManagerFactory = TslManager::setupTslManager;

    if (Configuration::instance().getOptionalBoolValue(ConfigurationKey::DEBUG_ENABLE_HSM_MOCK, false))
    {
#if WITH_HSM_MOCK > 0
        factories.hsmClientFactory = []() {
            return std::make_unique<HsmMockClient>();
        };
        factories.hsmFactoryFactory = [](std::unique_ptr<HsmClient>&&, std::shared_ptr<BlobCache> blobCache) {
            MockBlobCache::setupBlobCache(MockBlobCache::MockTarget::MockedHsm, *blobCache);
            return std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(), std::move(blobCache));
        };

        factories.teeTokenUpdaterFactory = TeeTokenUpdater::createMockTeeTokenUpdaterFactory();

        TLOG(INFO) << "Using MOCK security module.";
#else
        Fail("Mock HSM enabled, but it was not compiled in. "
             "To enable set cmake flag -DERP_WITH_HSM_MOCK=ON.");
#endif
    }
    else
    {
#if WITH_HSM_TPM_PRODUCTION > 0
        factories.hsmClientFactory = []() {
            return std::make_unique<HsmProductionClient>();
        };
        factories.hsmFactoryFactory = [](std::unique_ptr<HsmClient>&& client, std::shared_ptr<BlobCache> blobCache) {
            return std::make_unique<HsmProductionFactory>(std::move(client), std::move(blobCache));
        };
        factories.teeTokenUpdaterFactory = TeeTokenUpdater::createProductionTeeTokenUpdaterFactory();
#else
        Fail2("production HSM/TPM not compiled in", std::logic_error);
#endif
    }

#if WITH_HSM_TPM_PRODUCTION > 0
    factories.tpmFactory = TpmProduction::createFactory();
#else
    factories.tpmFactory = TpmMock::createFactory();
#endif

    factories.adminServerFactory = [](const std::string_view address, uint16_t port,
                                      RequestHandlerManager&& requestHandlers, BaseServiceContext& serviceContext,
                                      bool enforceClientAuthentication, const SafeString& caCertificates) {
        return std::make_unique<HttpsServer>(address, port, std::move(requestHandlers),
                                             dynamic_cast<MedicationExporterServiceContext&>(serviceContext),
                                             enforceClientAuthentication, caCertificates);
    };

    factories.exporterDatabaseFactory = [](HsmPool& hsmPool, KeyDerivation& keyDerivation) {
        return std::make_unique<MedicationExporterDatabaseFrontend>(
            std::make_unique<MedicationExporterPostgresBackend>(), hsmPool, keyDerivation);
    };

    factories.erpDatabaseFactory = [](HsmPool& hsmPool, KeyDerivation& keyDerivation) {
        return std::make_unique<exporter::MainDatabaseFrontend>(
            std::make_unique<exporter::MainPostgresBackend>(), hsmPool, keyDerivation);
    };

    factories.enrolmentServerFactory = [](const std::string_view address, uint16_t port,
                                          RequestHandlerManager&& requestHandlers, BaseServiceContext& serviceContext,
                                          bool enforceClientAuthentication, const SafeString& caCertificates) {
        return std::make_unique<HttpsServer>(address, port, std::move(requestHandlers),
                                             dynamic_cast<MedicationExporterServiceContext&>(serviceContext),
                                             enforceClientAuthentication, caCertificates);
    };

    factories.xmlValidatorFactory = [] {
        auto xmlValidator = std::make_shared<XmlValidator>();
        configureXmlValidator(*xmlValidator);
        return xmlValidator;
    };

    return factories;
}


int MedicationExporterMain::runApplication(
    MedicationExporterFactories&& factories, MainStateCondition& state,
    const std::function<void(MedicationExporterServiceContext&)>& postInitializationCallback /* = {} */)
{
#define log TLOG(INFO) << "Initialization: "

    const auto& configuration = Configuration::instance();
    configuration.check();

    auto ioThreadCount = static_cast<size_t>(configuration.getIntValue(ConfigurationKey::MEDICATION_EXPORTER_SERVER_IO_THREAD_COUNT));
    auto ioThreadPool = ThreadPool();

    ioThreadPool.setUp(ioThreadCount, "medication-exporter-io");
    auto& ioContext = ioThreadPool.ioContext();

    // access the Fhir instance to load the structure definitions now.
    log << "initializing FHIR data processing";
    Fhir::init<ConfigurationBase::MedicationExporter>(Fhir::Init::now);

    RunLoop runLoop;
    auto serviceContext =
        std::make_shared<MedicationExporterServiceContext>(ioContext, Configuration::instance(), std::move(factories));

    log << "setting up signal handler";
    SignalHandler signalHandler(runLoop.getThreadPool().ioContext());
    signalHandler.registerSignalHandlers({SIGINT, SIGTERM});

   log << "starting admin server";
   serviceContext->getAdminServer()->serve(1, "admin");

    if (serviceContext->getEnrolmentServer())
    {
        log << "starting enrolment server";
        // Number of requests is expected to be very low with days between small groups of requests.
        // Also, the TPM effectively being a singleton without multi-threading support, using a single server
        // thread is now a hard requirement.
        const size_t threadCount = 1;
        serviceContext->getEnrolmentServer()->serve(threadCount, "enroll");
    }

    log << "starting the medication exporter server " << std::this_thread::get_id();
    const size_t threadCount =
        static_cast<size_t>(configuration.getIntValue(ConfigurationKey::MEDICATION_EXPORTER_SERVER_THREAD_COUNT));
    Expect(threadCount > 0, "thread count is negative or zero");

    const std::chrono::seconds blobCacheRefreshInterval{
        configuration.getIntValue(ConfigurationKey::HSM_CACHE_REFRESH_SECONDS)};
    serviceContext->getBlobCache()->startRefresher(ioContext, blobCacheRefreshInterval);

    if (postInitializationCallback)
    {
        log << "calling post initialization callback";
        postInitializationCallback(*serviceContext);
    }
    TVLOG(0) << "serving requests with " << threadCount << " threads";

    runLoop.getThreadPool().setUp(threadCount, "medication-exporter");

    if (waitForHealthUp(runLoop, *serviceContext))
    {
        log << "complete";
        std::cout << "press CTRL-C to stop" << std::endl;
        runLoop.serve(serviceContext, threadCount);
    }

    runLoop.getThreadPool().joinAllThreads();

    if (serviceContext->getEnrolmentServer())
    {
        // Because the enrolment server runs in its own io context, it has to be terminated separately.
        serviceContext->getEnrolmentServer()->shutDown();
    }
    runLoop.shutDown();
    // According to glibc documentation SIGTERM "is the normal way to politely ask a program to terminate."
    // Therefore, it is not treated as error.
    TerminationHandler::instance().notifyTerminationCallbacks(signalHandler.mSignal != SIGTERM);

    // make sure the service context is deleted before shutting down the ioThreads
    // as the service context the io context
    serviceContext.reset();
    ioThreadPool.shutDown();

    state = MainState::Terminated;

    if (TerminationHandler::instance().hasError())
        return EXIT_FAILURE;
    else
        return EXIT_SUCCESS;
}

MedicationExporterMain::RunLoop::RunLoop()
    : mWorkerThreadPool()
{
}

void MedicationExporterMain::RunLoop::serve(const std::shared_ptr<MedicationExporterServiceContext>& serviceContext, const size_t threadCount)
{
    // Schedule initial job to the workers.
    for (size_t i = 0; i < threadCount; ++i)
    {
        boost::asio::co_spawn(mWorkerThreadPool.ioContext(), [this, serviceContext = std::weak_ptr{serviceContext}]() -> boost::asio::awaitable<void> {
            boost::asio::steady_timer timer{mWorkerThreadPool.ioContext()};
            while (! mWorkerThreadPool.ioContext().stopped())
            {
                Reschedule rescheduleRule{Reschedule::Delayed};
                {
                    auto serviceCtx = serviceContext.lock();
                    if (! serviceCtx)
                    {
                        co_return;
                    }

                    std::unique_ptr<EventProcessorInterface> eventProcessor = std::make_unique<EventProcessor>(serviceCtx);
                    rescheduleRule = eventProcessor->process() ? Reschedule::Immediate : Reschedule::Delayed;
                }
                if (rescheduleRule == Reschedule::Delayed)
                {
                    timer.expires_after(std::chrono::seconds(5));
                    co_await timer.async_wait(boost::asio::as_tuple(boost::asio::deferred));
                }
            }
        }, boost::asio::detached);
    }
}

void MedicationExporterMain::RunLoop::shutDown()
{
    mWorkerThreadPool.shutDown();
}

bool MedicationExporterMain::RunLoop::isStopped() const
{
    return mWorkerThreadPool.ioContext().stopped();
}

ThreadPool& MedicationExporterMain::RunLoop::getThreadPool()
{
    return mWorkerThreadPool;
}

bool MedicationExporterMain::waitForHealthUp(RunLoop& runLoop, MedicationExporterServiceContext& serviceContext)
{
    constexpr size_t loopWaitSeconds = 1;
    constexpr size_t loopMessageInterval = 2;
    constexpr size_t loopHealthCheckInterval = 5;

    size_t loopCount = 0;

    co_spawn(runLoop.getThreadPool().ioContext(), setupEpaClientPool(serviceContext), boost::asio::use_future).get();

    bool healthCheckIsUp = false;

    while (! runLoop.getThreadPool().ioContext().stopped())
    {
        if (loopCount > 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(loopWaitSeconds));
        }
        if (loopCount % loopHealthCheckInterval == 0)
        {
            TVLOG(1) << "running health check";
            try
            {
                serviceContext.getHsmPool().getTokenUpdater().healthCheck();
                // validate ePA endpoints
                if (testEpaEndpoints(serviceContext))
                {
                    healthCheckIsUp = true;
                }
            }
            catch (...)
            {
                JsonLog(LogId::INTERNAL_WARNING, JsonLog::makeErrorLogReceiver())
                    .message("Retry #" + std::to_string(0) + " health check due to resource temporarily unavailable ");
            }
        }
        if (healthCheckIsUp)
        {
            break;
        }

        ++loopCount;
        if (loopCount % loopMessageInterval == 1)
        {
            TLOG(WARNING) << "health status is DOWN, waiting for it to go up";
        }
    }

    if (healthCheckIsUp)
    {
        TLOG(INFO) << "health status is UP";
    }

    return healthCheckIsUp;
}


bool MedicationExporterMain::testEpaEndpoints(MedicationExporterServiceContext& serviceContext)
{
    auto epaHostPortList =
        Configuration::instance().getArray(ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_EPA_AS_FQDN);
    Expect(! epaHostPortList.empty(), "ePA Host list must not be empty");
    for (const auto& entry : epaHostPortList)
    {
        auto urlParts = UrlHelper::parseUrl(entry);
        auto epaClient =
            EpaMedicationClientImpl{serviceContext.ioContext(), urlParts.mHost, serviceContext.teeClientPool()};
        if (! epaClient.testConnection())
        {
            TLOG(WARNING) << "Connection ePA server at " << entry << " failed";
            return false;
        }
    }
    return true;
}