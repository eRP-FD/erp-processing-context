#include "test/workflow-test/EndpointTestClient.hxx"

#include "erp/ErpProcessingContext.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/hsm/BlobCache.hxx"
#include "erp/hsm/production/ProductionBlobDatabase.hxx"
#include "erp/idp/IdpUpdater.hxx"
#include "erp/service/DosHandler.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/hsm/MockBlobDatabase.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/mock/MockIdpUpdater.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/util/StaticData.hxx"


EndpointTestClient::EndpointTestClient()
    : mMockDatabase()
{
    RequestHandlerManager<PcServiceContext> handlers;
    ErpProcessingContext::addPrimaryEndpoints(handlers);

    auto blobCache = std::make_shared<BlobCache>(std::make_unique<MockBlobDatabase>());
    MockBlobCache::setupBlobCache(MockBlobCache::MockTarget::MockedHsm, *blobCache);
    auto hsmPool = std::make_unique<HsmPool>(
        std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(), std::move(blobCache)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory());
    mMockDatabase = std::make_unique<MockDatabase>(*hsmPool);

    auto serviceContext = std::make_unique<PcServiceContext>(
        Configuration::instance(), MockDatabaseProxy::createFactory(*mMockDatabase),
        std::make_unique<DosHandler>(std::make_unique<MockRedisStore>()), std::move(hsmPool),
        StaticData::getJsonValidator(), StaticData::getXmlValidator());

    if (TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_IDP_UPDATER_MOCK, false))
        IdpUpdater::create<MockIdpUpdater>(serviceContext->idp, serviceContext->getTslManager());
    else
        IdpUpdater::create(serviceContext->idp, serviceContext->getTslManager());


    // Test of client authentication:
    const bool enableClientAuthentication =
        TestConfiguration::instance().getBoolValue(TestConfigurationKey::TEST_ENABLE_CLIENT_AUTHENTICATON);
    const SafeString clientCertificate{
        TestConfiguration::instance().getStringValue(TestConfigurationKey::TEST_CLIENT_CERTIFICATE)};

    mServer = std::make_unique<HttpsServer<PcServiceContext>>(getHostAddress(), getPort(), std::move(handlers),
                                                              std::move(serviceContext), enableClientAuthentication,
                                                              clientCertificate);

    using namespace std::chrono_literals;
    mServer->serviceContext()->setPrngSeeder(std::make_unique<SeedTimer>(
        mServer->getThreadPool(), mServer->serviceContext()->getHsmPool(),
        Configuration::instance().getIntValue(ConfigurationKey::SERVER_THREAD_COUNT), 200ms, [](const SafeString&) {}));
    const_cast<SeedTimer*>(mServer->serviceContext()->getPrngSeeder())->refreshSeeds();

    mServer->serve(2);

    const SafeString serverCertificate{Configuration::instance().getStringValue(ConfigurationKey::SERVER_CERTIFICATE)};
    const SafeString clientPrivateKey{
        TestConfiguration::instance().getStringValue(TestConfigurationKey::TEST_CLIENT_PRIVATE_KEY)};
    mHttpsClient = std::make_unique<HttpsClient>(getHostAddress(), getPort(), 30/*httpClientConnectTimeoutSeconds*/,
                                                 true, serverCertificate, clientCertificate, clientPrivateKey);
}


ClientResponse EndpointTestClient::send(const ClientRequest& clientRequest)
{
    return mHttpsClient->send(clientRequest);
}


std::string EndpointTestClient::getHostHttpHeader() const
{
    return getHostAddress() + ':' + std::to_string(getPort());
}


std::string EndpointTestClient::getHostAddress() const
{
    return mLocalHost;
}


uint16_t EndpointTestClient::getPort() const
{
    return mPort;
}


EndpointTestClient::~EndpointTestClient()
{
    mServer->shutDown();
}
