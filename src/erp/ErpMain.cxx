/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/ErpMain.hxx"

#include "ErpRequirements.hxx"
#include "erp/ErpProcessingContext.hxx"
#include "erp/common/Constants.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/database/RedisClient.hxx"
#include "erp/enrolment/EnrolmentServer.hxx"
#include "erp/enrolment/EnrolmentServiceContext.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/hsm/production/HsmProductionClient.hxx"
#include "erp/hsm/production/HsmProductionFactory.hxx"
#include "erp/hsm/production/ProductionBlobDatabase.hxx"
#include "erp/hsm/production/TeeTokenProductionUpdater.hxx"
#include "erp/idp/IdpUpdater.hxx"
#include "erp/pc/SeedTimer.hxx"
#include "erp/tpm/Tpm.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/health/HealthCheck.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Holidays.hxx"
#include "erp/util/SignalHandler.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/TerminationHandler.hxx"
#include "erp/validation/JsonValidator.hxx"
#include "erp/validation/XmlValidator.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"


#if !defined __APPLE__ && !defined _WIN32
#include "erp/tpm/TpmProduction.hxx"
#else
#include "mock/tpm/TpmMock.hxx"
#endif

#include <chrono>
#include <iostream>
#include <stdexcept>

#if WITH_HSM_MOCK > 0
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/tpm/TpmTestHelper.hxx"
#endif

namespace
{
    std::optional<uint16_t> getPortFromConfiguration (const Configuration& configuration, const ConfigurationKey key)
    {
        const auto port = configuration.getOptionalIntValue(key);
        if (port.has_value())
        {
            Expect(port.value() > 0 && port.value() <= 65535, "default port number is out of range");
            return std::optional<uint16_t>(static_cast<uint16_t>(port.value()));
        }
        else
            return {};
    }
}


std::unique_ptr<HeartbeatSender> ErpMain::setupHeartbeatSender(
    const uint16_t port,
    HeartbeatSender::RegistrationFactory&& registrationFactory)
{
    std::unique_ptr<HeartbeatSender> sender;
    if (!Configuration::instance().getOptionalBoolValue(ConfigurationKey::DEBUG_DISABLE_REGISTRATION, false))
    {
        LOG(WARNING) << "Initializing Registration Heartbeating.";
        sender = HeartbeatSender::create(port, Configuration::instance(), std::move(registrationFactory));
        sender->start();
    }
    else
    {
        LOG(WARNING) << "Registration Heartbeating is disabled.";
    }
    return sender;
}


std::unique_ptr<SeedTimer> ErpMain::setupPrngSeeding(
    ThreadPool& threadpool,
    size_t threadCount,
    HsmPool& hsmPool)
{
    A_19021.start("reseed periodically to maintain 120 bit entropy");
    LOG(WARNING) << "Initializing Periodic Random Seeding.";

    std::chrono::seconds entropyFetchInterval{
        Configuration::instance().getOptionalIntValue(ConfigurationKey::ENTROPY_FETCH_INTERVAL_SECONDS, 60)};

    auto seedTimer = std::make_unique<SeedTimer>(threadpool,
                                                 hsmPool,
                                                 threadCount,
                                                 entropyFetchInterval);
    seedTimer->start(threadpool.ioContext(), std::chrono::seconds(0));

    A_19021.finish();

    return seedTimer;
}


std::unique_ptr<ErpProcessingContext> ErpMain::setupTeeServer (
    const uint16_t port,
    std::unique_ptr<HsmFactory> hsmFactory,
    Database::Factory&& databaseFactory,
    HsmPool::TeeTokenUpdaterFactory&& teeTokenUpdaterFactory,
    TslManager::TslManagerFactory&& tslManagerFactory,
    std::unique_ptr<RedisInterface> redisClient)
{
    const auto& configuration = Configuration::instance();
    auto jsonValidator = std::make_shared<JsonValidator>();
    jsonValidator->loadSchema(
        configuration.getArray(ConfigurationKey::JSON_SCHEMA),
        configuration.getPathValue(ConfigurationKey::JSON_META_SCHEMA));

    auto xmlValidator = std::make_shared<XmlValidator>();
    xmlValidator->loadSchemas(configuration.getArray(ConfigurationKey::XML_SCHEMA));

    auto tslManager = tslManagerFactory(xmlValidator);

    // There is only one service context for the whole runtime of the service
    auto serviceContext = std::make_unique<PcServiceContext>(
        configuration,
        std::move(databaseFactory),
        std::move(redisClient),
        std::make_unique<HsmPool>(
            std::move(hsmFactory),
            std::move(teeTokenUpdaterFactory)),
        std::move(jsonValidator),
        std::move(xmlValidator),
        std::move(tslManager));

    if (serviceContext->getTslManager() != nullptr)
    {
        X509Certificate cFdSigErp = X509Certificate::createFromBase64(serviceContext->getCFdSigErp().toDerBase64String());
        serviceContext->getTslManager()->addPostUpdateHook(
            [tslManager = serviceContext->getTslManager(), cFdSigErp] (void) mutable -> void
            {
                // failure by verification of ERP C.FD.SIG means that the application is not functional any more
                // so the exception can be thrown further by the hook ( that would lead to application termination )
                tslManager->verifyCertificate(
                    TslMode::TSL, cFdSigErp, {CertificateType::C_FD_SIG});
            });
    }

    RequestHandlerManager<PcServiceContext> teeHandlers;
    ErpProcessingContext::addPrimaryEndpoints(teeHandlers);

    auto processingContext = std::make_unique<ErpProcessingContext>(port, std::move(serviceContext), std::move(teeHandlers));
    Expect(processingContext!=nullptr, "failed to construct ErpProcessingContext");

    return processingContext;
}


std::unique_ptr<EnrolmentServer> ErpMain::setupEnrolmentServer (
    const uint16_t enrolmentPort,
    const std::shared_ptr<BlobCache>& blobCache)
{
    RequestHandlerManager<EnrolmentServiceContext> handlers;
    EnrolmentServer::addEndpoints(handlers);

    auto enrolmentServer = std::make_unique<EnrolmentServer>(
        enrolmentPort,
        std::move(handlers),
        #if !defined __APPLE__ && !defined _WIN32
        std::make_unique<EnrolmentServiceContext>(TpmProduction::createFactory(), blobCache));
        #else
        std::make_unique<EnrolmentServiceContext>(TpmMock::createFactory(), blobCache));
        #endif

    // Number of requests is expected to be very low with days between small groups of requests.
    // Also, the TPM effectively being a singleton without multi-threading support, using a single server
    // thread is now a hard requirement.
    const size_t threadCount = 1;
    enrolmentServer->getServer().serve(threadCount);

    return enrolmentServer;
}


int ErpMain::runApplication (
    uint16_t defaultEnrolmentServerPort,
    Factories&& factories,
    StateCondition& state,
    std::function<void(PcServiceContext&)> postInitializationCallback)
{
#define log LOG(WARNING) << "Initialization: "

    log << "setting up signal handler";
    state = State::Initializing;

    // trigger construction of singleton, before starting any thread
    (void)Holidays::instance();

    log << "starting TEE server";
    auto& configuration = Configuration::instance();
    const auto pcPort = configuration.serverPort();
    auto blobCache = factories.blobCacheFactory();
    auto hsmClient = factories.hsmClientFactory();
    auto hsmFactory = factories.hsmFactoryFactory(std::move(hsmClient), blobCache);
    auto processingContext = setupTeeServer(
        pcPort,
        std::move(hsmFactory),
        std::move(factories.databaseFactory),
        std::move(factories.teeTokenUpdaterFactory),
        std::move(factories.tslManagerFactory),
        factories.redisClientFactory());

    // Setup the IDP updater. As this triggers a first, synchronous update, the IDP updater has to be started
    // before the TEE server accepts connections.
    log << "starting IDP update";
    auto& serviceContext = *processingContext->getServer().serviceContext();
    auto idpUpdater = IdpUpdater::create(
        serviceContext.idp,
        serviceContext.getTslManager());

    log << "starting enrolment server";
    std::unique_ptr<EnrolmentServer> enrolmentServer;
    const auto enrolmentPort = getEnrolementServerPort(pcPort, defaultEnrolmentServerPort);
    if (enrolmentPort.has_value())
        enrolmentServer = setupEnrolmentServer(enrolmentPort.value(), blobCache);

    // access the Fhir instance to load the structure definitions now.
    log << "initializing FHIR data processing";
    Fhir::instance();

    log << "starting the TEE server";
    const int threadCount = configuration.getOptionalIntValue(ConfigurationKey::SERVER_THREAD_COUNT, 10);
    Expect(threadCount>0, "thread count is negative or zero");
    processingContext->getServer().serve(threadCount);

    // Start periodically seeding and updating seeds in all worker threads.
    // Started after the TEE server because a) it runs asynchronously anyway and b) it will access the thread pool which
    // therefore has to be initialized first.
    log << "starting PRNG seeder";
    auto& hsmPool = serviceContext.getHsmPool();
    auto prngSeeder = setupPrngSeeding(processingContext->getServer().getThreadPool(), threadCount, hsmPool);
    serviceContext.setPrngSeeder(std::move(prngSeeder));

    if (postInitializationCallback)
    {
        log << "calling post initialization callback";
        postInitializationCallback(serviceContext);
    }

    // At this point both servers, tee/VAU and enrolment are up and accepting requests.
    // We wait and block the main thread until background configuration is complete and the health check
    // has found all required services.
    log << "waiting for health check to report all services as up";
    waitForHealthUp(serviceContext);

    SignalHandler signalHandler(processingContext->getServer().getThreadPool().ioContext());
    signalHandler.registerSignalHandlers({SIGINT, SIGTERM});


    log << "complete";
    std::cout << "press CTRL-C to stop" << std::endl;

    // Advertise the tee server as available via redis at the vau proxy.
    log << "setting up heartbeat sender";
    auto heartbeatSender = setupHeartbeatSender(pcPort, std::move(factories.registrationFactory));

    state = State::WaitingForTermination;
    processingContext->getServer().waitForShutdown();
    if (enrolmentServer)
    {
        // Because the enrolment server runs in its own io context, it has to be terminated separately.
        enrolmentServer->getServer().shutDown();
    }

    // According to glibc documentation SIGTERM "is the normal way to politely ask a program to terminate."
    // Therefore it is not treated as error.
    TerminationHandler::instance().notifyTerminationCallbacks(signalHandler.mSignal != SIGTERM);

    state = State::Terminated;

    if (TerminationHandler::instance().hasError())
        return EXIT_FAILURE;
    else
        return EXIT_SUCCESS;
}


ErpMain::Factories ErpMain::createProductionFactories()
{
    ErpMain::Factories factories;

    TVLOG(0) << "HSM mock is configured as "
        << (Configuration::instance().getOptionalBoolValue(ConfigurationKey::DEBUG_ENABLE_HSM_MOCK, false) ? "ON" : "OFF");
    TVLOG(0) << "HSM mock is compiled "
    #if WITH_HSM_MOCK > 0
        << "IN";
    #else
        << "OUT";
    #endif

    factories.registrationFactory = HeartbeatSender::createDefaultRegistrationFactory();
    factories.databaseFactory = [](HsmPool& hsmPool, KeyDerivation& keyDerivation)
        {
            return std::make_unique<DatabaseFrontend>(std::make_unique<PostgresBackend>(), hsmPool, keyDerivation);
        };

    factories.blobCacheFactory = []
    {
        return std::make_shared<BlobCache>(
            std::make_unique<ProductionBlobDatabase>());
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

        TLOG(WARNING) << "Using builtin security module.";
#else
        Fail(
            "Mock HSM enabled, but it was not compiled in. "
            "To enable set cmake flag -DERP_WITH_HSM_MOCK=ON."
        );
#endif
    }
    else
    {
        factories.hsmClientFactory = []()
        {
#if !defined __APPLE__ && !defined _WIN32
            return std::make_unique<HsmProductionClient>();
#else
            return std::make_unique<HsmMockClient>();
#endif
        };
        factories.hsmFactoryFactory = [](std::unique_ptr<HsmClient>&& client, std::shared_ptr<BlobCache> blobCache)
        {
            return std::make_unique<HsmProductionFactory>(std::move(client), std::move(blobCache));
        };
        factories.teeTokenUpdaterFactory = TeeTokenUpdater::createProductionTeeTokenUpdaterFactory();
    }

    factories.redisClientFactory =
        []{
            // Note, that TestConfigurationKey::TEST_USE_REDIS_MOCK is ignored for production setup.
            return std::make_unique<RedisClient>();
        };

    return factories;
}


std::shared_ptr<TslManager> ErpMain::setupTslManager(const std::shared_ptr<XmlValidator>& xmlValidator)
{
    LOG(WARNING) << "Initializing TSL-Manager";

    std::shared_ptr<TslManager> tslManager;

    try
    {
        std::string tslSslRootCa;
        const std::string tslRootCaFile =
            Configuration::instance().getOptionalStringValue(ConfigurationKey::TSL_FRAMEWORK_SSL_ROOT_CA_PATH, "");
        if ( ! tslRootCaFile.empty())
        {
            tslSslRootCa = FileHelper::readFileAsString(tslRootCaFile);
        }

        tslManager = std::make_shared<TslManager>(
            std::make_shared<UrlRequestSender>(
                SafeString(std::move(tslSslRootCa)),
                static_cast<uint16_t>(Configuration::instance().getOptionalIntValue(
                    ConfigurationKey::HTTPCLIENT_CONNECT_TIMEOUT_SECONDS, Constants::httpTimeoutInSeconds))),
            xmlValidator);
    }
    catch (const TslError& e)
    {
        LOG(ERROR) << "Can not create TslManager, TslError: " << e.what();
        throw;
    }
    catch (const std::exception& e)
    {
        LOG(ERROR) << "Can not create TslManager, unexpected exception: " << e.what();
        throw;
    }

    return tslManager;
}


std::optional<uint16_t> ErpMain::getEnrolementServerPort (
    const uint16_t pcPort,
    const uint16_t defaultEnrolmentServerPort)
{
    const auto enrolmentPort = getPortFromConfiguration(Configuration::instance(), ConfigurationKey::ENROLMENT_SERVER_PORT)
        .value_or(defaultEnrolmentServerPort);
    const auto activeForPcPort = Configuration::instance().getOptionalIntValue(
        ConfigurationKey::ENROLMENT_ACTIVATE_FOR_PORT,
        -1);
    if (activeForPcPort != pcPort)
    {
        LOG(WARNING) << "Enrolment-Server (on port=" << enrolmentPort << ") is disabled (ERP_SERVER_PORT = " << pcPort
                     <<") != (ENROLMENT_ACTIVATE_FOR_PORT = "<< activeForPcPort << ")";
    }

    return activeForPcPort == pcPort ? std::optional<uint16_t>(enrolmentPort) : std::nullopt;
}


void ErpMain::waitForHealthUp (PcServiceContext& serviceContext)
{
    constexpr size_t loopWaitSeconds = 1;
    constexpr size_t loopMessageInterval = 2;
    constexpr size_t loopHealthCheckInterval = 5;
    size_t loopCount = 0;

    auto request = ServerRequest(Header());
    auto response = ServerResponse();
    AccessLog accessLog;
    accessLog.discard();  // This accessLog object is only used to satisfy the SessionContext constructor.
    SessionContext<PcServiceContext> session (serviceContext, request, response, accessLog);

    while (true)
    {
        if (loopCount % loopHealthCheckInterval == 0)
        {
            TVLOG(1) << "running health check";
            HealthCheck::update(session);
        }

        if (session.serviceContext.applicationHealth().isUp())
            break;

        ++loopCount;
        if (loopCount % loopMessageInterval == 1)
            LOG(WARNING) << "health status is DOWN (" << session.serviceContext.applicationHealth().downServicesString()
                         << "), waiting for it to go up";

        std::this_thread::sleep_for(std::chrono::seconds(loopWaitSeconds));
    }

    if (loopCount > 0)
        LOG(WARNING) << "health status is UP";
}
