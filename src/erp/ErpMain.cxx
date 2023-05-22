/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/ErpMain.hxx"

#include "ErpRequirements.hxx"
#include "erp/admin/AdminServer.hxx"
#include "erp/admin/AdminRequestHandler.hxx"
#include "erp/ErpProcessingContext.hxx"
#include "erp/common/Constants.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/database/RedisClient.hxx"
#include "erp/enrolment/EnrolmentServer.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/hsm/VsdmKeyCache.hxx"
#include "erp/hsm/production/ProductionBlobDatabase.hxx"
#include "erp/hsm/production/ProductionVsdmKeyBlobDatabase.hxx"
#include "erp/idp/IdpUpdater.hxx"
#include "erp/pc/SeedTimer.hxx"
#include "erp/registration/ApplicationHealthAndRegistrationUpdater.hxx"
#include "erp/registration/RegistrationManager.hxx"
#include "erp/tpm/Tpm.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/Condition.hxx"
#include "erp/util/health/HealthCheck.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Holidays.hxx"
#include "erp/util/SignalHandler.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/TerminationHandler.hxx"
#include "erp/validation/InCodeValidator.hxx"
#include "erp/validation/JsonValidator.hxx"
#include "erp/validation/XmlValidator.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"


#if WITH_HSM_MOCK > 0
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/tpm/TpmMock.hxx"
#include "mock/tpm/TpmTestHelper.hxx"
#include "mock/idp/MockIdpUpdater.hxx"
#include "mock/tsl/MockTslManager.hxx"
#endif
#if WITH_HSM_TPM_PRODUCTION > 0
#include "erp/hsm/production/HsmProductionClient.hxx"
#include "erp/hsm/production/HsmProductionFactory.hxx"
#include "erp/hsm/production/TeeTokenProductionUpdater.hxx"
#include "erp/tpm/TpmProduction.hxx"
#endif


#include <chrono>
#include <iostream>
#include <stdexcept>

std::unique_ptr<ApplicationHealthAndRegistrationUpdater> ErpMain::setupHeartbeatSender(PcServiceContext& serviceContext)
{
    std::unique_ptr<ApplicationHealthAndRegistrationUpdater> sender;
    if (!Configuration::instance().getOptionalBoolValue(ConfigurationKey::DEBUG_DISABLE_REGISTRATION, false))
    {
        TLOG(INFO) << "Initializing Registration Heartbeating.";
        sender = ApplicationHealthAndRegistrationUpdater::create(Configuration::instance(), serviceContext,
                                         serviceContext.registrationInterface());
        sender->start();
    }
    else
    {
        TLOG(INFO) << "Registration Heartbeating is disabled.";
    }
    return sender;
}


// GEMREQ-start A_19021-02#setupPrngSeeding
std::unique_ptr<SeedTimer> ErpMain::setupPrngSeeding(
    ThreadPool& threadpool,
    HsmPool& randomSource)
{
    A_19021_02.start("reseed periodically to maintain 120 bit entropy");
    TLOG(INFO) << "Initializing Periodic Random Seeding.";

    std::chrono::seconds entropyFetchInterval{
        Configuration::instance().getOptionalIntValue(ConfigurationKey::ENTROPY_FETCH_INTERVAL_SECONDS, 60)};

    auto seedTimer = std::make_unique<SeedTimer>(threadpool,
                                                 randomSource,
                                                 entropyFetchInterval);
    seedTimer->start(threadpool.ioContext(), std::chrono::seconds(0));

    A_19021_02.finish();

    return seedTimer;
}
// GEMREQ-end A_19021-02#setupPrngSeeding


int ErpMain::runApplication (
    Factories&& factories,
    StateCondition& state,
    const std::function<void(PcServiceContext&)>& postInitializationCallback)
{
#define log TLOG(INFO) << "Initialization: "

    const auto& configuration = Configuration::instance();
    configuration.check();

    log << "setting up signal handler";
    state = State::Initializing;

    // trigger construction of singleton, before starting any thread
    (void)Holidays::instance();

    // access the Fhir instance to load the structure definitions now.
    log << "initializing FHIR data processing";
    (void)Fhir::instance();

    // There is only one service context for the whole runtime of the service

    auto serviceContext = std::make_shared<PcServiceContext>(Configuration::instance(), std::move(factories));
    auto& ioContext = serviceContext->getTeeServer().getThreadPool().ioContext();

    const auto schemaVersion = Version(serviceContext->databaseFactory()->retrieveSchemaVersion());
    const auto expectedSchemaVersion = Version(Database::expectedSchemaVersion);
    if(schemaVersion < expectedSchemaVersion)
    {
        TLOG(ERROR) << "Warning: Invalid database schema version '" << schemaVersion << "', server expects version '"
                   <<  Database::expectedSchemaVersion << "'";
    }
    else if(schemaVersion > expectedSchemaVersion)
    {
        TLOG(ERROR) << "Warning: Current database schema version '" << schemaVersion << "' is higher than expected version '"
                   <<  Database::expectedSchemaVersion << "'";
    }

    SignalHandler signalHandler(ioContext);
    signalHandler.registerSignalHandlers({SIGINT, SIGTERM});

    log << "starting admin server";
    serviceContext->getAdminServer().serve(1, "admin");


    // Setup the IDP updater. As this triggers a first, synchronous update, the IDP updater has to be started
    // before the TEE server accepts connections.
    log << "starting IDP update";
#if WITH_HSM_MOCK > 0
    auto idpUpdater = IdpUpdater::create<MockIdpUpdater>(
        serviceContext->idp,
        serviceContext->getTslManager(),
        serviceContext->getTimerManager());
#else
    auto idpUpdater = IdpUpdater::create(
        serviceContext->idp,
        serviceContext->getTslManager(),
        serviceContext->getTimerManager());
#endif

    if (serviceContext->getEnrolmentServer())
    {
        log << "starting enrolment server";
        // Number of requests is expected to be very low with days between small groups of requests.
        // Also, the TPM effectively being a singleton without multi-threading support, using a single server
        // thread is now a hard requirement.
        const size_t threadCount = 1;
        serviceContext->getEnrolmentServer()->serve(threadCount, "enroll");
    }

    log << "starting the TEE server";
    const size_t threadCount =
        static_cast<size_t>(configuration.getOptionalIntValue(ConfigurationKey::SERVER_THREAD_COUNT, 10));
    Expect(threadCount>0, "thread count is negative or zero");
    serviceContext->getTeeServer().serve(threadCount, "vau");

    const std::chrono::seconds blobCacheRefreshInterval{
            configuration.getIntValue(ConfigurationKey::HSM_CACHE_REFRESH_SECONDS)};
    serviceContext->getBlobCache()->startRefresher(ioContext, blobCacheRefreshInterval);
    serviceContext->getVsdmKeyCache().startRefresher(ioContext, blobCacheRefreshInterval);

    // Start periodically seeding and updating seeds in all worker threads.
    // Started after the TEE server because a) it runs asynchronously anyway and b) it will access the thread pool which
    // therefore has to be initialized first.
    log << "starting PRNG seeder";
    auto& hsmPool = serviceContext->getHsmPool();
    auto prngSeeder = setupPrngSeeding(serviceContext->getTeeServer().getThreadPool(), hsmPool);
    serviceContext->setPrngSeeder(std::move(prngSeeder));

    if (postInitializationCallback)
    {
        log << "calling post initialization callback";
        postInitializationCallback(*serviceContext);
    }

    // At this point both servers, tee/VAU and enrolment are up and accepting requests.
    // We wait and block the main thread until background configuration is complete and the health check
    // has found all required services.
    log << "waiting for health check to report all services as up";
    std::unique_ptr<ApplicationHealthAndRegistrationUpdater> heartbeatSender;
    if(waitForHealthUp(*serviceContext))
    {
        log << "complete";
        std::cout << "press CTRL-C to stop" << std::endl;

        // Advertise the tee server as available via redis at the vau proxy.
        log << "setting up heartbeat sender";
        heartbeatSender = setupHeartbeatSender(*serviceContext);
    }
    state = State::WaitingForTermination;
    serviceContext->getTeeServer().waitForShutdown();
    if (serviceContext->getEnrolmentServer())
    {
        // Because the enrolment server runs in its own io context, it has to be terminated separately.
        serviceContext->getEnrolmentServer()->shutDown();
    }
    serviceContext->getAdminServer().shutDown();

    // According to glibc documentation SIGTERM "is the normal way to politely ask a program to terminate."
    // Therefore it is not treated as error.
    TerminationHandler::instance().notifyTerminationCallbacks(signalHandler.mSignal != SIGTERM);

    state = State::Terminated;

    if (TerminationHandler::instance().hasError())
        return EXIT_FAILURE;
    else
        return EXIT_SUCCESS;
}


Factories ErpMain::createProductionFactories()
{
    Factories factories;

    TVLOG(0) << "HSM/TPM mock is configured as "
        << (Configuration::instance().getOptionalBoolValue(ConfigurationKey::DEBUG_ENABLE_HSM_MOCK, false) ? "ON" : "OFF");
    TVLOG(0) << "HSM/TPM mock is compiled "
    #if WITH_HSM_MOCK > 0
        << "IN";
    #else
        << "OUT";
    #endif

    TVLOG(0) << "Production HSM/TPM is compiled "
#if WITH_HSM_TPM_PRODUCTION > 0
             << "IN";
#else
             << "OUT";
#endif

    factories.databaseFactory = [](HsmPool& hsmPool, KeyDerivation& keyDerivation)
        {
            return std::make_unique<DatabaseFrontend>(std::make_unique<PostgresBackend>(), hsmPool, keyDerivation);
        };

    factories.blobCacheFactory = []
    {
        return std::make_shared<BlobCache>(
            std::make_unique<ProductionBlobDatabase>());
    };

    factories.vsdmKeyBlobDatabaseFactory = []() {
        return std::make_unique<ProductionVsdmKeyBlobDatabase>();
    };

    factories.tslManagerFactory = ErpMain::setupTslManager;

    if (Configuration::instance().getOptionalBoolValue(ConfigurationKey::DEBUG_ENABLE_HSM_MOCK, false))
    {
#if WITH_HSM_MOCK > 0
        factories.hsmClientFactory = []()
        {
            return std::make_unique<HsmMockClient>();
        };
        factories.hsmFactoryFactory = [](std::unique_ptr<HsmClient>&&, std::shared_ptr<BlobCache> blobCache)
        {
            MockBlobCache::setupBlobCache(MockBlobCache::MockTarget::MockedHsm, *blobCache);
            return std::make_unique<HsmMockFactory>(
                std::make_unique<HsmMockClient>(),
                std::move(blobCache));
        };

        factories.teeTokenUpdaterFactory = TeeTokenUpdater::createMockTeeTokenUpdaterFactory();

        TLOG(INFO) << "Using MOCK security module.";
#else
        Fail(
            "Mock HSM enabled, but it was not compiled in. "
            "To enable set cmake flag -DERP_WITH_HSM_MOCK=ON."
        );
#endif
    }
    else
    {
#if WITH_HSM_TPM_PRODUCTION > 0
        factories.hsmClientFactory = []()
        {
            return std::make_unique<HsmProductionClient>();
        };
        factories.hsmFactoryFactory = [](std::unique_ptr<HsmClient>&& client, std::shared_ptr<BlobCache> blobCache)
        {
            return std::make_unique<HsmProductionFactory>(std::move(client), std::move(blobCache));
        };
        factories.teeTokenUpdaterFactory = TeeTokenUpdater::createProductionTeeTokenUpdaterFactory();
#else
        Fail2("production HSM/TPM not compiled in", std::logic_error);
#endif
    }

    factories.redisClientFactory =
        []{
            // Note, that TestConfigurationKey::TEST_USE_REDIS_MOCK is ignored for production setup.
            return std::make_unique<RedisClient>();
        };

    factories.teeServerFactory = [](const std::string_view address, uint16_t port,
                                    RequestHandlerManager&& requestHandlers,
                                    PcServiceContext& serviceContext, bool enforceClientAuthentication,
                                    const SafeString& caCertificates) {
        return std::make_unique<HttpsServer>(address, port, std::move(requestHandlers), serviceContext,
                                             enforceClientAuthentication, caCertificates);
    };
    factories.enrolmentServerFactory = factories.teeServerFactory;
    factories.adminServerFactory = factories.teeServerFactory;

#if WITH_HSM_TPM_PRODUCTION > 0
        factories.tpmFactory = TpmProduction::createFactory();
#else
        factories.tpmFactory = TpmMock::createFactory();
#endif

    factories.jsonValidatorFactory = []{
        auto jsonValidator = std::make_shared<JsonValidator>();
        jsonValidator->loadSchema(
            Configuration::instance().getArray(ConfigurationKey::JSON_SCHEMA),
            Configuration::instance().getPathValue(ConfigurationKey::JSON_META_SCHEMA));
        return jsonValidator;
    };

    factories.xmlValidatorFactory = []{
        auto xmlValidator = std::make_shared<XmlValidator>();
        configureXmlValidator(*xmlValidator);
        return xmlValidator;
    };

    factories.incodeValidatorFactory = [] {
        return std::make_shared<InCodeValidator>();
    };

    return factories;
}


std::shared_ptr<TslManager> ErpMain::setupTslManager(const std::shared_ptr<XmlValidator>& xmlValidator)
{
    TLOG(INFO) << "Initializing TSL-Manager";

    try
    {
        std::string tslSslRootCa;
        const std::string tslRootCaFile =
            Configuration::instance().getOptionalStringValue(ConfigurationKey::TSL_FRAMEWORK_SSL_ROOT_CA_PATH, "");
        if ( ! tslRootCaFile.empty())
        {
            tslSslRootCa = FileHelper::readFileAsString(tslRootCaFile);
        }

        const auto useMockTslManager =
            Configuration::instance().getOptionalBoolValue(ConfigurationKey::DEBUG_ENABLE_MOCK_TSL_MANAGER, false);

#if WITH_HSM_MOCK > 0
        if (useMockTslManager)
        {
            return MockTslManager::createMockTslManager(xmlValidator);
        }
#else
        Expect3(! useMockTslManager,
                "Configuration error: DEBUG_ENABLE_MOCK_TSL_MANAGER=true, but WITH_HSM_MOCK is not compiled in",
                std::logic_error);
#endif
        auto requestSender = std::make_shared<UrlRequestSender>(
            SafeString(std::move(tslSslRootCa)),
            static_cast<uint16_t>(Configuration::instance().getOptionalIntValue(
                ConfigurationKey::HTTPCLIENT_CONNECT_TIMEOUT_SECONDS, Constants::httpTimeoutInSeconds)));
        return std::make_shared<TslManager>(std::move(requestSender), xmlValidator);
    }
    catch (const TslError& e)
    {
        TLOG(ERROR) << "Can not create TslManager, TslError: " << e.what();
        throw;
    }
    catch (const std::exception& e)
    {
        TLOG(ERROR) << "Can not create TslManager, unexpected exception: " << e.what();
        throw;
    }
}


bool ErpMain::waitForHealthUp (PcServiceContext& serviceContext)
{
    constexpr size_t loopWaitSeconds = 1;
    constexpr size_t loopMessageInterval = 2;
    constexpr size_t loopHealthCheckInterval = 5;

    size_t loopCount = 0;

    bool healthCheckIsUp = false;

    while (!serviceContext.getTeeServer().isStopped())
    {
        if (loopCount % loopHealthCheckInterval == 0)
        {
            TVLOG(1) << "running health check";
            HealthCheck::update(serviceContext);
        }
        healthCheckIsUp = serviceContext.applicationHealth().isUp();
        if (healthCheckIsUp)
            break;

        ++loopCount;
        if (loopCount % loopMessageInterval == 1)
            TLOG(WARNING) << "health status is DOWN (" << serviceContext.applicationHealth().downServicesString()
                          << "), waiting for it to go up";

        std::this_thread::sleep_for(std::chrono::seconds(loopWaitSeconds));
    }

    if (healthCheckIsUp)
        TLOG(INFO) << "health status is UP";

    return healthCheckIsUp;
}
