/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/workflow-test/EndpointTestClient.hxx"
#include "erp/admin/AdminServer.hxx"
#include "erp/ErpProcessingContext.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/hsm/BlobCache.hxx"
#include "erp/hsm/production/ProductionBlobDatabase.hxx"
#include "erp/idp/IdpUpdater.hxx"
#include "erp/service/DosHandler.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/mock/MockIdpUpdater.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/util/StaticData.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/RegistrationMock.hxx"
#include "erp/enrolment/EnrolmentServer.hxx"


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
            initVauServer(xmlValidator);
            return;
        case Target::ENROLMENT:
            initEnrolmentServer();
            return;
    }
    Fail2("invalid value for Target: " + std::to_string(static_cast<intmax_t>(target)), std::logic_error);
}

void EndpointTestClient::initAdminServer()
{
    mPortGuard =
        std::make_unique<EnvironmentVariableGuard>(ConfigurationKey::ADMIN_SERVER_PORT, std::to_string(adminPort));
    const auto& config = Configuration::instance();
    RequestHandlerManager manager;
    AdminServer::addEndpoints(manager);
    auto factories = StaticData::makeMockFactories();
    factories.adminServerFactory = &EndpointTestClient::httpsServerFactory;
    mContext = std::make_unique<PcServiceContext>(config, std::move(factories));
    mContext->getAdminServer().serve(1);
    initClient();
}

void EndpointTestClient::initEnrolmentServer()
{
    mPortGuard = std::make_unique<EnvironmentVariableGuard>(ConfigurationKey::ENROLMENT_SERVER_PORT,
                                                            std::to_string(enrolmentPort));
    const auto& config = Configuration::instance();
    RequestHandlerManager manager;
    EnrolmentServer::addEndpoints(manager);
    auto factories = StaticData::makeMockFactories();
    factories.enrolmentServerFactory = &EndpointTestClient::httpsServerFactory;
    mContext = std::make_unique<PcServiceContext>(config, std::move(factories));
    mContext->getEnrolmentServer()->serve(1);
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
    factories.xmlValidatorFactory = [xmlValidator] {
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
        mServer->getThreadPool(), mContext->getHsmPool(),
        Configuration::instance().getIntValue(ConfigurationKey::SERVER_THREAD_COUNT), 200ms, [](const SafeString&) {}));
    const_cast<SeedTimer*>(mContext->getPrngSeeder())->refreshSeeds();

    mServer->serve(2);

    initClient();
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
    switch (mTarget)
    {
        case Target::ADMIN:
            return adminPort;
        case Target::VAU:
            return vauPort;
        case Target::ENROLMENT:
            return enrolmentPort;
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
