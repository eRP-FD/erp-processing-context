/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/workflow-test/EndpointTestClient.hxx"
#include "erp/ErpProcessingContext.hxx"
#include "erp/admin/AdminServer.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/enrolment/EnrolmentServer.hxx"
#include "erp/enrolment/VsdmHmacKey.hxx"
#include "erp/hsm/BlobCache.hxx"
#include "erp/hsm/VsdmKeyBlobDatabase.hxx"
#include "erp/hsm/production/ProductionBlobDatabase.hxx"
#include "erp/idp/IdpUpdater.hxx"
#include "erp/pc/SeedTimer.hxx"
#include "erp/service/DosHandler.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/idp/MockIdpUpdater.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/mock/RegistrationMock.hxx"
#include "test/util/StaticData.hxx"


EndpointTestClient::EndpointTestClient(std::shared_ptr<XmlValidator> xmlValidator, Target target)
    : mMockDatabase()
    , mTarget(target)
{
    switch (target)
    {
        case TestClient::Target::ADMIN:
            initAdminServer();
            return;
        case TestClient::Target::VAU:
            initVauServer(std::move(xmlValidator));
            return;
        case Target::ENROLMENT:
            initEnrolmentServer();
            return;
    }
    Fail2("invalid value for Target: " + std::to_string(static_cast<intmax_t>(target)), std::logic_error);
}

void EndpointTestClient::initAdminServer()
{
    const auto& config = Configuration::instance();
    RequestHandlerManager manager;
    AdminServer::addEndpoints(manager);
    auto factories = StaticData::makeMockFactories();
    factories.adminServerFactory = &EndpointTestClient::httpsServerFactory;
    mContext = std::make_unique<PcServiceContext>(config, std::move(factories));
    mContext->getAdminServer().serve(1, "admin-test");
    initClient();
}

void EndpointTestClient::initEnrolmentServer()
{
    const auto& config = Configuration::instance();
    RequestHandlerManager manager;
    EnrolmentServer::addEndpoints(manager);
    auto factories = StaticData::makeMockFactories();
    factories.enrolmentServerFactory = &EndpointTestClient::httpsServerFactory;
    mContext = std::make_unique<PcServiceContext>(config, std::move(factories));
    mContext->getEnrolmentServer()->serve(1, "enroll-test");
    initClient();
}

void EndpointTestClient::initClient()
{

    const SafeString serverCertificate{Configuration::instance().getStringValue(ConfigurationKey::SERVER_CERTIFICATE)};
    const SafeString clientCertificate{
        TestConfiguration::instance().getStringValue(TestConfigurationKey::TEST_CLIENT_CERTIFICATE)};
    const SafeString clientPrivateKey{
        TestConfiguration::instance().getStringValue(TestConfigurationKey::TEST_CLIENT_PRIVATE_KEY)};
    mHttpsClient = std::make_unique<HttpsClient>(getHostAddress(), getPort(), 30, true, serverCertificate,
                                                 clientCertificate, clientPrivateKey);
}

void EndpointTestClient::initVsdmKeys()
{
    const auto& vsdmKeys = TestConfiguration::instance().getArray(TestConfigurationKey::TEST_VSDM_KEYS);
    auto& vsdmKeyDb = mContext->getVsdmKeyBlobDatabase();
    auto hsmPoolSession = mPool->acquire();
    auto& hsmSession = hsmPoolSession.session();
    for (const auto& key : vsdmKeys)
    {
        VsdmHmacKey keyPackage{key};
        try
        {
            vsdmKeyDb.deleteBlob(keyPackage.operatorId(), keyPackage.version());
        }
        catch(std::exception&)
        {
        }
        const ErpVector vsdmKeyData = ErpVector::create(key);
        VsdmKeyBlobDatabase::Entry vsdmKeyBlobEntry;
        vsdmKeyBlobEntry.operatorId = keyPackage.operatorId();
        vsdmKeyBlobEntry.version = keyPackage.version();
        vsdmKeyBlobEntry.blob = hsmSession.wrapRawPayload(vsdmKeyData, 0);
        vsdmKeyBlobEntry.createdDateTime = std::chrono::system_clock::now();
        vsdmKeyDb.storeBlob(std::move(vsdmKeyBlobEntry));
    }
}


void EndpointTestClient::initVauServer(std::shared_ptr<XmlValidator> xmlValidator)
{
    RequestHandlerManager handlers;
    ErpProcessingContext::addPrimaryEndpoints(handlers);

    mPool = std::make_unique<HsmPool>(
        std::make_unique<HsmMockFactory>(
            std::make_unique<HsmMockClient>(),
            MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory(),
        std::make_shared<Timer>());
    mMockDatabase = std::make_unique<MockDatabase>(*mPool);

    auto factories = StaticData::makeMockFactories();
    factories.xmlValidatorFactory = [xmlValidator = std::move(xmlValidator)] {
        return xmlValidator;
    };
    factories.databaseFactory = createDatabaseFactory();

    mContext = std::make_unique<PcServiceContext>(Configuration::instance(), std::move(factories));

    if (TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_IDP_UPDATER_MOCK, false))
        IdpUpdater::create<MockIdpUpdater>(mContext->idp, mContext->getTslManager(), mContext->getTimerManager());
    else
        IdpUpdater::create(mContext->idp, mContext->getTslManager(), mContext->getTimerManager());


    // Test of client authentication:
    const bool enableClientAuthentication =
        TestConfiguration::instance().getBoolValue(TestConfigurationKey::TEST_ENABLE_CLIENT_AUTHENTICATON);
    const SafeString clientCertificate{
        TestConfiguration::instance().getStringValue(TestConfigurationKey::TEST_CLIENT_CERTIFICATE)};

    mServer = std::make_unique<HttpsServer>(getHostAddress(), getPort(), std::move(handlers),
                                            *mContext, enableClientAuthentication,
                                            clientCertificate);

    using namespace std::chrono_literals;
    mServer->serviceContext().setPrngSeeder(std::make_unique<SeedTimer>(
        mServer->getThreadPool(), mContext->getHsmPool(), 200ms, [](const SafeString&) {}));
    const_pointer_cast<SeedTimerHandler>(mContext->getPrngSeeder()->handler())->refreshSeeds();

    mServer->serve(2, "vau-test");

    initClient();
    initVsdmKeys();
}

Database::Factory EndpointTestClient::createDatabaseFactory()
{
    if (isPostgresEnabled())
    {
        return [](HsmPool& hsmPool, KeyDerivation& keyDerivation){
            return std::make_unique<DatabaseFrontend>(std::make_unique<PostgresBackend>(), hsmPool, keyDerivation); };
    }
    else
    {
        return [this](HsmPool& hsmPool, KeyDerivation& keyDerivation){
                return std::make_unique<DatabaseFrontend>(
                    std::make_unique<MockDatabaseProxy>(*mMockDatabase), hsmPool, keyDerivation);
            };
    }
}

bool EndpointTestClient::isPostgresEnabled()
{
    return TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false);
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
    const auto& config = Configuration::instance();
    switch (mTarget)
    {
        case Target::ADMIN:
            return gsl::narrow<uint16_t>(config.getIntValue(ConfigurationKey::ADMIN_SERVER_PORT));
        case Target::VAU:
            return config.serverPort();
        case Target::ENROLMENT:
            return gsl::narrow<uint16_t>(config.getIntValue(ConfigurationKey::ENROLMENT_SERVER_PORT));
    }
    Fail2("invalid value for Target: " + std::to_string(static_cast<intmax_t>(mTarget)), std::logic_error);
}


EndpointTestClient::~EndpointTestClient()
{
    if (mServer)
    {
        mServer->shutDown();
    }
    if (mTarget == TestClient::Target::ADMIN)
    {
        mContext->getAdminServer().shutDown();
    }
}

std::unique_ptr<TestClient> EndpointTestClient::factory(std::shared_ptr<XmlValidator> xmlValidator, Target target)
{
    return std::make_unique<EndpointTestClient>(std::move(xmlValidator), target);
}

std::unique_ptr<HttpsServer> EndpointTestClient::httpsServerFactory(const std::string_view address, uint16_t port,
                                                                    RequestHandlerManager&& requestHandlers,
                                                                    PcServiceContext& serviceContext,
                                                                    bool enforceClientAuthentication,
                                                                    const SafeString& caCertificates)
{
    return std::make_unique<HttpsServer>(address, port, std::move(requestHandlers), serviceContext,
                                         enforceClientAuthentication, caCertificates);
}

PcServiceContext* EndpointTestClient::getContext() const
{
    return mContext.get();
}
