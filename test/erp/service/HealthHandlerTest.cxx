/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include <gtest/gtest.h>// should be first or FRIEND_TEST would not work

#include "erp/service/HealthHandler.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/model/Health.hxx"
#include "erp/pc/SeedTimer.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/tsl/TslManager.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Hash.hxx"
#include "mock/hsm/HsmMockClient.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "test/erp/pc/CFdSigErpTestHelper.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/erp/service/HealthHandlerTestTslManager.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "mock/tsl/MockOcsp.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "mock/tsl/UrlRequestSenderMock.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/StaticData.hxx"
#include "erp/erp-serverinfo.hxx"

#include "test_config.h"
#include "test/mock/RegistrationMock.hxx"
#include "erp/registration/RegistrationManager.hxx"

using namespace std::chrono_literals;

class HealthHandlerTestMockDatabase : public MockDatabase
{
public:
    using MockDatabase::MockDatabase;

    void commitTransaction() override
    {
        MockDatabase::commitTransaction();
        if (fail)
        {
            throw std::runtime_error("CONNECTION FAILURE");
        }
    }

    static bool fail;
};
bool HealthHandlerTestMockDatabase::fail = false;

class HealthHandlerTestHsmMockClient : public HsmMockClient
{
public:
    ErpVector getRndBytes(const HsmRawSession& session, size_t input) override
    {
        if (! fail)
            return HsmMockClient::getRndBytes(session, input);
        throw std::runtime_error("FAILURE");
    }

    static bool fail;
};
bool HealthHandlerTestHsmMockClient::fail = false;

class HealthHandlerTestMockRedisStore : public MockRedisStore
{
public:
    void setKeyFieldValue(const std::string_view& key, const std::string_view& field,
                          const std::string_view& value) override
    {
        if (fail)
            throw std::runtime_error("REDIS FAILURE");
        MockRedisStore::setKeyFieldValue(key, field, value);
    }

    static bool fail;
};
bool HealthHandlerTestMockRedisStore::fail = false;

bool HealthHandlerTestTslManager::failTsl = false;
bool HealthHandlerTestTslManager::failBna = false;
bool HealthHandlerTestTslManager::failOcspRetrieval = false;

class HealthHandlerTestSeedTimerMock : public SeedTimerHandler
{
public:
    using SeedTimerHandler::SeedTimerHandler;
    void healthCheck() const override
    {
        if (fail)
            throw std::runtime_error("SEEDTIMER FAILURE");
    }
    static bool fail;
};
bool HealthHandlerTestSeedTimerMock::fail = false;


class HealthHandlerTestTeeTokenUpdater : public TeeTokenUpdater
{
public:
    explicit HealthHandlerTestTeeTokenUpdater(TokenConsumer&& teeTokenConsumer, HsmFactory& hsmFactory,
                                              TokenProvider&& tokenProvider, std::shared_ptr<Timer> timerManager)
        : TeeTokenUpdater(std::move(teeTokenConsumer), hsmFactory, std::move(tokenProvider), std::move(timerManager), 100ms, 100ms)
    {
    }
};


class HealthHandlerTestTeeTokenUpdaterFactory
{
public:
    static TeeTokenUpdater::TeeTokenUpdaterFactory createHealthHandlerTestMockTeeTokenUpdaterFactory()
    {
        return [](auto&& tokenConsumer, auto& hsmFactory, std::shared_ptr<Timer> timerManager) {
            return std::make_unique<HealthHandlerTestTeeTokenUpdater>(
                std::forward<decltype(tokenConsumer)>(tokenConsumer), hsmFactory, [](HsmFactory&) {
                    if (HealthHandlerTestTeeTokenUpdaterFactory::fail)
                    {
                        throw std::runtime_error("TEE TOKEN UPDATER FAILURE");
                    }
                    // Tests that use a mock HSM don't need a TEE token. An empty blob is enough.
                    return ErpBlob();
                },
                timerManager);
        };
    }

    static bool fail;
};
bool HealthHandlerTestTeeTokenUpdaterFactory::fail = false;


class HealthHandlerTest : public testing::Test
{
public:
    HealthHandlerTest()
        : mBlobCache(MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm))
        , mServiceContext()
        , request({})
        , response()
        , mContext()
        , statusPointer("/status")
        , postgresStatusPointer("/checks/0/status")
        , postgresRootCausePointer("/checks/0/data/rootCause")
        , hsmStatusPointer("/checks/2/status")
        , hsmRootCausePointer("/checks/2/data/rootCause")
        , hsmIpPointer("/checks/2/data/ip")
        , redisStatusPointer("/checks/1/status")
        , redisRootCausePointer("/checks/1/data/rootCause")
        , tslStatusPointer("/checks/3/status")
        , tslRootCausePointer("/checks/3/data/rootCause")
        , bnaStatusPointer("/checks/4/status")
        , bnaRootCausePointer("/checks/4/data/rootCause")
        , idpStatusPointer("/checks/5/status")
        , idpRootCausePointer("/checks/5/data/rootCause")
        , seedTimerStatusPointer("/checks/6/status")
        , seedTimerRootCausePointer("/checks/6/data/rootCause")
        , teeTokenUpdaterStatusPointer("/checks/7/status")
        , teeTokenUpdaterRootCausePointer("/checks/7/data/rootCause")
        , cFdSigErpPointer("/checks/8/status")
        , cFdSigErpRootCausePointer("/checks/8/data/rootCause")
        , cFdSigErpTimestampPointer("/checks/8/data/timestamp")
        , cFdSigErpPolicyPointer("/checks/8/data/policy")
        , cFdSigErpExpiryPointer("/checks/8/data/certificate_expiry")
        , buildPointer("/version/build")
        , buildTypePointer("/version/buildType")
        , releasePointer("/version/release")
        , releasedatePointer("/version/releasedate")
        , mGuardERP_HSM_DEVICE("ERP_HSM_DEVICE", "127.0.0.1")
        , mGuardERP_TSL_INITIAL_CA_DER_PATH("ERP_TSL_INITIAL_CA_DER_PATH",
                                            ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der"))
    {
        createServiceContext();
    }

    void createServiceContext()
    {
        const auto cert = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErp());
        const auto certCA = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErpSigner());
        const std::string ocspUrl(CFdSigErpTestHelper::cFsSigErpOcspUrl());

        auto factories = StaticData::makeMockFactories();
        factories.databaseFactory = [](HsmPool& hsmPool, KeyDerivation& keyDerivation) {
            auto md = std::make_unique<HealthHandlerTestMockDatabase>(hsmPool);
            return std::make_unique<DatabaseFrontend>(std::move(md), hsmPool, keyDerivation);
        };

        factories.redisClientFactory = []{return std::make_unique<HealthHandlerTestMockRedisStore>();};
        factories.hsmClientFactory = []{return std::make_unique<HealthHandlerTestHsmMockClient>();};
        factories.teeTokenUpdaterFactory = HealthHandlerTestTeeTokenUpdaterFactory::createHealthHandlerTestMockTeeTokenUpdaterFactory();
        factories.tslManagerFactory = [cert, certCA, ocspUrl](const std::shared_ptr<XmlValidator> &){
            return TslTestHelper::createTslManager<HealthHandlerTestTslManager>(
                CFdSigErpTestHelper::createRequestSender<UrlRequestSenderMock>(),
                {},
                {{ocspUrl, {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});
        };
        factories.blobCacheFactory = [this](){return mBlobCache;};
        mServiceContext = std::make_unique<PcServiceContext>(Configuration::instance(), std::move(factories));
        mContext = std::make_unique<SessionContext>(*mServiceContext, request, response, mAccessLog);

        mPool.setUp(1, "test");
        using namespace std::chrono_literals;
        auto mockHandler = std::make_shared<HealthHandlerTestSeedTimerMock>(
            mPool, mServiceContext->getHsmPool(), 200ms, [](const SafeString&) {});
        mServiceContext->setPrngSeeder(std::make_unique<SeedTimer>(std::move(mockHandler)));
    }

    void handleRequest(bool withIdpUpdate = true)
    {
        if (withIdpUpdate)
        {
            // dummy update to set IDP green
            mContext->serviceContext.idp.setCertificate(Certificate(shared_X509()));
        }
        mHandler.handleRequest(*mContext);
    }

    ~HealthHandlerTest() override = default;

    void verifyRootCause (
        rapidjson::Document& document,
        const rapidjson::Pointer& pointer,
        const std::string& expectedValue)
    {
        const std::string value = pointer.Get(document)->GetString();
        EXPECT_TRUE(value.find(expectedValue) != std::string::npos)
            << "Actual: " << value << "\nExpected to contain: " << expectedValue;
    }

protected:
    std::shared_ptr<BlobCache> mBlobCache;
    std::unique_ptr<PcServiceContext> mServiceContext;
    ServerRequest request;
    ServerResponse response;
    AccessLog mAccessLog;
    std::unique_ptr<SessionContext> mContext;
    ThreadPool mPool;
    HealthHandler mHandler;
    rapidjson::Pointer statusPointer;
    rapidjson::Pointer postgresStatusPointer;
    rapidjson::Pointer postgresRootCausePointer;
    rapidjson::Pointer hsmStatusPointer;
    rapidjson::Pointer hsmRootCausePointer;
    rapidjson::Pointer hsmIpPointer;
    rapidjson::Pointer redisStatusPointer;
    rapidjson::Pointer redisRootCausePointer;
    rapidjson::Pointer tslStatusPointer;
    rapidjson::Pointer tslRootCausePointer;
    rapidjson::Pointer bnaStatusPointer;
    rapidjson::Pointer bnaRootCausePointer;
    rapidjson::Pointer idpStatusPointer;
    rapidjson::Pointer idpRootCausePointer;
    rapidjson::Pointer seedTimerStatusPointer;
    rapidjson::Pointer seedTimerRootCausePointer;
    rapidjson::Pointer teeTokenUpdaterStatusPointer;
    rapidjson::Pointer teeTokenUpdaterRootCausePointer;
    rapidjson::Pointer cFdSigErpPointer;
    rapidjson::Pointer cFdSigErpRootCausePointer;
    rapidjson::Pointer cFdSigErpTimestampPointer;
    rapidjson::Pointer cFdSigErpPolicyPointer;
    rapidjson::Pointer cFdSigErpExpiryPointer;
    rapidjson::Pointer buildPointer;
    rapidjson::Pointer buildTypePointer;
    rapidjson::Pointer releasePointer;
    rapidjson::Pointer releasedatePointer;
    EnvironmentVariableGuard mGuardERP_HSM_DEVICE;
    EnvironmentVariableGuard mGuardERP_TSL_INITIAL_CA_DER_PATH;
};

TEST_F(HealthHandlerTest, healthy)//NOLINT(readability-function-cognitive-complexity)
{

    ASSERT_NO_THROW(handleRequest());

    ASSERT_EQ(mContext->response.getHeader().status(), HttpStatus::OK);
    ASSERT_FALSE(mContext->response.getBody().empty());

    rapidjson::Document healthDocument;
    auto body = mContext->response.getBody();
    healthDocument.Parse(body);

    EXPECT_EQ(std::string(statusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(postgresStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(hsmStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_FALSE(std::string(hsmIpPointer.Get(healthDocument)->GetString()).empty());
    EXPECT_EQ(std::string(hsmIpPointer.Get(healthDocument)->GetString()),
              Configuration::instance().getStringValue(ConfigurationKey::HSM_DEVICE));
    EXPECT_EQ(std::string(redisStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(tslStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(bnaStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(idpStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(cFdSigErpPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_NE(std::string(cFdSigErpTimestampPointer.Get(healthDocument)->GetString()), "never successfully validated");
    EXPECT_EQ(std::string(cFdSigErpPolicyPointer.Get(healthDocument)->GetString()), "C.FD.OSIG");
    EXPECT_EQ(std::string(cFdSigErpExpiryPointer.Get(healthDocument)->GetString()), "2024-02-14T23:00:00.000+00:00");
    EXPECT_EQ(std::string(seedTimerStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(teeTokenUpdaterStatusPointer.Get(healthDocument)->GetString()),
              std::string(model::Health::up));
    EXPECT_EQ(std::string(ErpServerInfo::BuildVersion()), std::string(buildPointer.Get(healthDocument)->GetString()));
    EXPECT_EQ(std::string(ErpServerInfo::BuildType()), std::string(buildTypePointer.Get(healthDocument)->GetString()));
    EXPECT_EQ(std::string(ErpServerInfo::ReleaseVersion()), std::string(releasePointer.Get(healthDocument)->GetString()));
    EXPECT_EQ(std::string(ErpServerInfo::ReleaseDate()), std::string(releasedatePointer.Get(healthDocument)->GetString()));

    EXPECT_TRUE(mContext->serviceContext.registrationInterface()->registered());
}


TEST_F(HealthHandlerTest, DatabaseDown)//NOLINT(readability-function-cognitive-complexity)
{
    HealthHandlerTestMockDatabase::fail = true;
    ASSERT_NO_THROW(handleRequest());
    HealthHandlerTestMockDatabase::fail = false;

    ASSERT_EQ(mContext->response.getHeader().status(), HttpStatus::OK);
    ASSERT_FALSE(mContext->response.getBody().empty());

    rapidjson::Document healthDocument;
    auto body = mContext->response.getBody();
    healthDocument.Parse(body);

    ASSERT_EQ(std::string(statusPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    ASSERT_EQ(std::string(postgresStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    verifyRootCause(healthDocument, postgresRootCausePointer, "CONNECTION FAILURE");
    EXPECT_FALSE(mContext->serviceContext.registrationInterface()->registered());
}

TEST_F(HealthHandlerTest, HsmDown)//NOLINT(readability-function-cognitive-complexity)
{
    HealthHandlerTestHsmMockClient::fail = true;
    ASSERT_NO_THROW(handleRequest());
    HealthHandlerTestHsmMockClient::fail = false;

    ASSERT_EQ(mContext->response.getHeader().status(), HttpStatus::OK);

    rapidjson::Document healthDocument;
    auto body = mContext->response.getBody();
    healthDocument.Parse(body);

    EXPECT_EQ(std::string(statusPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    EXPECT_EQ(std::string(hsmStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    verifyRootCause(healthDocument, hsmRootCausePointer, "FAILURE");
    EXPECT_EQ(std::string(hsmIpPointer.Get(healthDocument)->GetString()),
              Configuration::instance().getStringValue(ConfigurationKey::HSM_DEVICE));
    EXPECT_FALSE(mContext->serviceContext.registrationInterface()->registered());
}

TEST_F(HealthHandlerTest, RedisDown)//NOLINT(readability-function-cognitive-complexity)
{
    HealthHandlerTestMockRedisStore::fail = true;
    EnvironmentVariableGuard envGuard("DEBUG_DISABLE_DOS_CHECK", "false");
    ASSERT_NO_THROW(handleRequest());
    HealthHandlerTestMockRedisStore::fail = false;

    ASSERT_EQ(mContext->response.getHeader().status(), HttpStatus::OK);

    rapidjson::Document healthDocument;
    auto body = mContext->response.getBody();
    healthDocument.Parse(body);

    EXPECT_EQ(std::string(statusPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    EXPECT_EQ(std::string(redisStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    verifyRootCause(healthDocument, redisRootCausePointer, "REDIS FAILURE");
    EXPECT_FALSE(mContext->serviceContext.registrationInterface()->registered());
}

TEST_F(HealthHandlerTest, TslDown)//NOLINT(readability-function-cognitive-complexity)
{
    HealthHandlerTestTslManager::failTsl = true;
    EnvironmentVariableGuard envGuard("DEBUG_IGNORE_TSL_CHECKS", "false");
    ASSERT_NO_THROW(handleRequest());
    HealthHandlerTestTslManager::failTsl = false;

    ASSERT_EQ(mContext->response.getHeader().status(), HttpStatus::OK);

    rapidjson::Document healthDocument;
    auto body = mContext->response.getBody();
    healthDocument.Parse(body);

    EXPECT_EQ(std::string(statusPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    EXPECT_EQ(std::string(tslStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    verifyRootCause(healthDocument, tslRootCausePointer, "TSL FAILURE");
    EXPECT_FALSE(mContext->serviceContext.registrationInterface()->registered());
}

TEST_F(HealthHandlerTest, BnaDown)//NOLINT(readability-function-cognitive-complexity)
{
    HealthHandlerTestTslManager::failBna = true;
    EnvironmentVariableGuard envGuard("DEBUG_IGNORE_TSL_CHECKS", "false");
    ASSERT_NO_THROW(handleRequest());
    HealthHandlerTestTslManager::failBna = false;

    ASSERT_EQ(mContext->response.getHeader().status(), HttpStatus::OK);

    rapidjson::Document healthDocument;
    auto body = mContext->response.getBody();
    healthDocument.Parse(body);

    EXPECT_EQ(std::string(statusPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    EXPECT_EQ(std::string(bnaStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    verifyRootCause(healthDocument, bnaRootCausePointer, "BNA FAILURE");
    EXPECT_FALSE(mContext->serviceContext.registrationInterface()->registered());
}

TEST_F(HealthHandlerTest, IdpDown)//NOLINT(readability-function-cognitive-complexity)
{
    ASSERT_NO_THROW(handleRequest(false));

    ASSERT_EQ(mContext->response.getHeader().status(), HttpStatus::OK);

    rapidjson::Document healthDocument;
    auto body = mContext->response.getBody();
    healthDocument.Parse(body);

    EXPECT_EQ(std::string(statusPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    EXPECT_EQ(std::string(idpStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    verifyRootCause(healthDocument, idpRootCausePointer, "never updated");
    EXPECT_FALSE(mContext->serviceContext.registrationInterface()->registered());
}

TEST_F(HealthHandlerTest, CFdSigErpDown)//NOLINT(readability-function-cognitive-complexity)
{
    HealthHandlerTestTslManager::failOcspRetrieval = true;
    ASSERT_THROW(mServiceContext->getCFdSigErpManager().getOcspResponseData(true), std::runtime_error);
    ASSERT_NO_THROW(handleRequest());
    HealthHandlerTestTslManager::failOcspRetrieval = false;

    ASSERT_EQ(mContext->response.getHeader().status(), HttpStatus::OK);

    rapidjson::Document healthDocument;
    auto body = mContext->response.getBody();
    healthDocument.Parse(body);

    // sub-status DOWN does not cause the application health to go down.
    EXPECT_EQ(std::string(statusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(cFdSigErpPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    EXPECT_NE(std::string(cFdSigErpTimestampPointer.Get(healthDocument)->GetString()), "never successfully validated");
    EXPECT_EQ(std::string(cFdSigErpPolicyPointer.Get(healthDocument)->GetString()), "C.FD.OSIG");
    EXPECT_EQ(std::string(cFdSigErpExpiryPointer.Get(healthDocument)->GetString()), "2024-02-14T23:00:00.000+00:00");
    verifyRootCause(healthDocument, cFdSigErpRootCausePointer, "no successful validation available");
    EXPECT_TRUE(mContext->serviceContext.registrationInterface()->registered());
}

TEST_F(HealthHandlerTest, SeedTimerDown)//NOLINT(readability-function-cognitive-complexity)
{
    HealthHandlerTestSeedTimerMock::fail = true;
    ASSERT_NO_THROW(handleRequest());
    HealthHandlerTestSeedTimerMock::fail = false;

    ASSERT_EQ(mContext->response.getHeader().status(), HttpStatus::OK);

    rapidjson::Document healthDocument;
    auto body = mContext->response.getBody();
    healthDocument.Parse(body);

    EXPECT_EQ(std::string(statusPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    EXPECT_EQ(std::string(seedTimerStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    verifyRootCause(healthDocument, seedTimerRootCausePointer, "SEEDTIMER FAILURE");
    EXPECT_FALSE(mContext->serviceContext.registrationInterface()->registered());
}

TEST_F(HealthHandlerTest, TeeTokenUpdaterDown)//NOLINT(readability-function-cognitive-complexity)
{
    HealthHandlerTestTeeTokenUpdaterFactory::fail = true;
    std::this_thread::sleep_for(151ms);
    ASSERT_NO_THROW(handleRequest());
    HealthHandlerTestTeeTokenUpdaterFactory::fail = false;

    ASSERT_EQ(mContext->response.getHeader().status(), HttpStatus::OK);

    rapidjson::Document healthDocument;
    auto body = mContext->response.getBody();
    healthDocument.Parse(body);

    EXPECT_EQ(std::string(statusPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    EXPECT_EQ(std::string(teeTokenUpdaterStatusPointer.Get(healthDocument)->GetString()),
              std::string(model::Health::down));
    verifyRootCause(healthDocument, teeTokenUpdaterRootCausePointer, "last update is too old");
    EXPECT_FALSE(mContext->serviceContext.registrationInterface()->registered());
}

TEST_F(HealthHandlerTest, VauSigBlobMissing)//NOLINT(readability-function-cognitive-complexity)
{
    mBlobCache->deleteBlob(BlobType::VauSig, ErpVector::create("vau-sig"));
    createServiceContext();
    ASSERT_NO_THROW(handleRequest());

    ASSERT_EQ(mContext->response.getHeader().status(), HttpStatus::OK);

    rapidjson::Document healthDocument;
    auto body = mContext->response.getBody();
    healthDocument.Parse(body);

    EXPECT_EQ(std::string(statusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(cFdSigErpPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    EXPECT_NE(std::string(cFdSigErpTimestampPointer.Get(healthDocument)->GetString()), "never successfully validated");
    EXPECT_EQ(std::string(cFdSigErpPolicyPointer.Get(healthDocument)->GetString()), "");
    EXPECT_EQ(std::string(cFdSigErpExpiryPointer.Get(healthDocument)->GetString()), "");
    verifyRootCause(
        healthDocument, cFdSigErpRootCausePointer,
        "std::runtime_error(16ExceptionWrapperISt13runtime_errorE)(no successful validation available) at ");

    EXPECT_TRUE(mContext->serviceContext.registrationInterface()->registered());
}
