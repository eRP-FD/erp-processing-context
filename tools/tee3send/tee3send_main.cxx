#include "exporter/EpaAccountLookupClient.hxx"
#include "exporter/TelematikLookup.hxx"
#include "exporter/client/EpaMedicationClientImpl.hxx"
#include "exporter/database/MedicationExporterDatabaseFrontend.hxx"
#include "exporter/database/MedicationExporterPostgresBackend.hxx"
#include "exporter/network/client/Tee3Client.hxx"
#include "exporter/network/client/Tee3ClientPool.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "exporter/server/HttpsServer.hxx"
#include "shared/hsm/production/ProductionBlobDatabase.hxx"
#include "shared/hsm/production/ProductionVsdmKeyBlobDatabase.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/erp-serverinfo.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Demangle.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/GLogConfiguration.hxx"

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

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast/http/write.hpp>
#include <shared/enrolment/EnrolmentHelper.hxx>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <span>
#include <string>
#include <vector>


struct ShowHelp : std::runtime_error {
    using std::runtime_error::runtime_error;
};

MedicationExporterFactories createProductionFactories()
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
        return std::make_unique<exporter::HttpsServer>(address, port, std::move(requestHandlers),
                                             dynamic_cast<MedicationExporterServiceContext&>(serviceContext),
                                             enforceClientAuthentication, caCertificates);
    };

    std::stringstream ss;
    static TelematikLookup telematikLookup(ss);
    factories.exporterDatabaseFactory = [](KeyDerivation& keyDerivation) {
        return std::make_unique<MedicationExporterDatabaseFrontend>(
            std::make_unique<MedicationExporterPostgresBackend>(), keyDerivation, telematikLookup);
    };

    factories.erpDatabaseFactory = [](HsmPool& hsmPool, KeyDerivation& keyDerivation) {
        return std::make_unique<exporter::MainDatabaseFrontend>(
            std::make_unique<exporter::MainPostgresBackend>(), hsmPool, keyDerivation);
    };

    factories.enrolmentServerFactory = [](const std::string_view address, uint16_t port,
                                          RequestHandlerManager&& requestHandlers, BaseServiceContext& serviceContext,
                                          bool enforceClientAuthentication, const SafeString& caCertificates) {
        return std::make_unique<exporter::HttpsServer>(address, port, std::move(requestHandlers),
                                             dynamic_cast<MedicationExporterServiceContext&>(serviceContext),
                                             enforceClientAuthentication, caCertificates);
    };

    factories.xmlValidatorFactory = [] {
        auto xmlValidator = std::make_shared<XmlValidator>();
        configureXmlValidator(*xmlValidator);
        return xmlValidator;
    };

    factories.erpDatabaseFactory = [](HsmPool&, KeyDerivation&) -> std::unique_ptr<exporter::MainDatabaseFrontend> {
        Fail2("tee3send must not use database", std::logic_error);
    };

    return factories;
}

boost::asio::awaitable<void> setupHosts(std::shared_ptr<Tee3ClientPool> clientPool, std::string host, uint16_t port)
{
    co_await clientPool->addEpaHost(host, port, 1);
}

boost::asio::awaitable<void> runTeeClient(std::shared_ptr<Tee3ClientPool> clientPool, std::string host, std::chrono::seconds waitTime)
{
    auto executor = co_await boost::asio::this_coro::executor;

    LOG(INFO) << "waiting " << waitTime;
    boost::asio::steady_timer timer{executor, waitTime};
    co_await timer.async_wait(boost::asio::deferred);
    auto teeClient = co_await clientPool->acquire(host);
    teeClient.reset();
    //model::Kvnr kvnr{"X1234567890"};
    model::Kvnr kvnr{"Y0114130000"};
    using boost::beast::http::verb;
    try
    {
        std::unordered_map<std::string, std::any> empty{};
        auto req = Tee3Client::Request{verb::post, "/epa/medication/api/v1/fhir/$provide-prescription-erp", 11};
        req.set(Header::Tee3::XInsurantId, kvnr.id());
        req.set(Header::XRequestId, Uuid().toString());
        req.set(Header::ContentType, static_cast<std::string>(MimeType::fhirJson));
        req.body() = "{}";
        auto response = co_await clientPool->sendTeeRequest(host, Uuid{}.toString(), req, empty);
        LOG(INFO) << "got response after waiting " << waitTime;
    }
    catch (const std::exception& e)
    {
        LOG(ERROR) << "call failed with " << e.what();
    }
}

void testHttpsClient(MedicationExporterServiceContext& serviceContext)
{
    const auto& config = Configuration::instance();
    const auto& userAgent = config.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_USER_AGENT);
    auto fqdns = config.epaFQDNs();
    auto fqdn = fqdns.front();
    model::Kvnr kvnr{"X1234567890"};
    for (int i = 0; i < 10; ++i)
    {
        std::thread([&]() {
            auto client = EpaAccountLookupClient(serviceContext, "/information/api/v1/ehr/consentdecisions", userAgent);
            LOG(INFO) << "sleeping... ";
            std::this_thread::sleep_for(std::chrono::seconds{1});
            client.sendConsentDecisionsRequest(Uuid{}.toString(), kvnr, fqdn.hostName, gsl::narrow<std::uint16_t>(fqdn.port));
            LOG(INFO) << "finished consent... ";
        }).detach();
    }
    std::this_thread::sleep_for(std::chrono::seconds{15});

    for (int i = 0; i < 10; ++i)
    {
        std::thread([&]() {
            auto client = EpaAccountLookupClient(serviceContext, "/information/api/v1/ehr/consentdecisions", userAgent);
            LOG(INFO) << "sleeping... ";
            std::this_thread::sleep_for(std::chrono::seconds{1});
            client.sendConsentDecisionsRequest(Uuid{}.toString(), kvnr, fqdn.hostName, gsl::narrow<std::uint16_t>(fqdn.port));
            LOG(INFO) << "finished consent... ";
        }).detach();
    }
    std::this_thread::sleep_for(std::chrono::seconds{10});
}


int main(int argc, const char* argv[])
{
    auto args = std::span(argv, size_t(argc));
    try
    {
        GLogConfiguration::initLogging(args[0]);

        Expect3(argc < 3, "too many arguments", ShowHelp);
        Expect3(argc >= 2, "missing argument", ShowHelp);

        std::string_view hostPort{args[1]};
        size_t colon = hostPort.find(':');
        Expect3(colon != std::string_view::npos, "missing colon in <host>:<port>", ShowHelp);
        std::string host{hostPort.substr(0, colon)};
        std::string_view portStr{hostPort.substr(colon + 1)};
        Expect3(! host.empty(), "<host> must not be empty", ShowHelp);
        Expect3(! portStr.empty(), "<port> must not be empty", ShowHelp);
        uint16_t port{};
        auto res = std::from_chars(portStr.begin(), portStr.end(), port);
        Expect3(res.ec == std::errc{}, "invalid port: " + make_error_code(res.ec).message(), ShowHelp);
        Expect3(res.ptr == portStr.end(), "extra characters in port: " + std::string{portStr}, ShowHelp);

        boost::asio::io_context ioContext{1};
        auto factories = createProductionFactories();
        Environment::set("ERP_MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_EPA_AS_FQDN", std::string{hostPort});
        auto serviceContext = std::make_shared<MedicationExporterServiceContext>(ioContext, Configuration::instance(),
                                                                                 std::move(factories));
        // Run the tee token updater once, now, so that we have a valid Tee token.
        EnrolmentHelper::refreshTeeToken(serviceContext->getHsmPool());
        // testHttpsClient(*serviceContext);

        auto workGuard = boost::asio::make_work_guard(ioContext);
        auto ioThread = std::thread([&] {
            try
            {
                ioContext.run();
            }
            catch (const std::exception& e)
            {
                LOG(ERROR) << "caught exception in io context: " << e.what();
            }
        });
        LOG(INFO) << "setup pool";
        auto clientPool =
            std::make_shared<Tee3ClientPool>(ioContext, serviceContext->getHsmPool(), serviceContext->getTslManager());
        LOG(INFO) << "setup hosts";
        co_spawn(ioContext, setupHosts(clientPool, host, port), boost::asio::use_future).get();

        for (int i = 1; i < 10; ++i)
        {
            co_spawn(ioContext, runTeeClient(clientPool, host, std::chrono::seconds{i * 40}), boost::asio::detached);
        }

        co_spawn(ioContext, runTeeClient(clientPool, host, std::chrono::seconds{0}), boost::asio::detached);

        //
        // uncomment below to use EpaMedicationClient directly
        //
        // auto epa = EpaMedicationClientImpl(ioContext, host, clientPool);
        // LOG(INFO) << "sending request";
        // model::Kvnr kvnr{"X1234567890"};

        // auto result = epa.sendProvideDispensation(kvnr, "test");
        // LOG(INFO) << result.body.serializeToJsonString();

        clientPool.reset();
        workGuard.reset();
        if (ioThread.joinable()) {
            ioThread.join();
        }
    }
    catch (const ShowHelp& sh)
    {
        std::cerr << sh.what() << "\n\n";
        std::cerr << "usage: " << args[0] << " <host>:<port>"
                  << "\n";
        return 1;
    }
    catch (const ExceptionWrapperBase& ew)
    {
        const std::exception* ex = dynamic_cast<const std::exception*>(&ew);
        std::cerr << ew.location.rootLocation.fileName << ":" << ew.location.rootLocation.line << ": "
                  << (ex ? ex->what() : util::demangle(typeid(ew).name())) << std::endl;
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
    }
    return 0;
}
