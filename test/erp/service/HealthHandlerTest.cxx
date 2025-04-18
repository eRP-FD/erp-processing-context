/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/HealthHandler.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/pc/SeedTimer.hxx"
#include "erp/registration/RegistrationManager.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "mock/hsm/HsmMockClient.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/tsl/MockOcsp.hxx"
#include "mock/tsl/UrlRequestSenderMock.hxx"
#include "shared/erp-serverinfo.hxx"
#include "shared/model/Health.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "shared/tsl/TslManager.hxx"
#include "shared/tsl/error/TslError.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/Hash.hxx"
#include "test_config.h"
#include "test/erp/pc/CFdSigErpTestHelper.hxx"
#include "test/erp/service/HealthHandlerTestTslManager.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/mock/RegistrationMock.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>// should be first or FRIEND_TEST would not work

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
    explicit HealthHandlerTestTeeTokenUpdater(HsmPool& hsmPool, TokenRefresher&& tokenRefresher,
                                              std::shared_ptr<Timer> timerManager)
        : TeeTokenUpdater(hsmPool, std::move(tokenRefresher), std::move(timerManager), 100ms, 100ms)
    {
    }
};


class HealthHandlerTestTeeTokenUpdaterFactory
{
public:
    static TeeTokenUpdater::TeeTokenUpdaterFactory createHealthHandlerTestMockTeeTokenUpdaterFactory()
    {
        return [](auto& hsmPool, std::shared_ptr<Timer> timerManager) {
            return std::make_unique<HealthHandlerTestTeeTokenUpdater>(
                hsmPool,
                [](HsmPool&) {
                    if (HealthHandlerTestTeeTokenUpdaterFactory::fail)
                    {
                        throw std::runtime_error("TEE TOKEN UPDATER FAILURE");
                    }
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
        , currentTimestampPointer("/timestamp")
        // Note that the array indices is defined in ApplicationHealth::model()
        , postgresStatusPointer("/checks/3/status")
        , postgresRootCausePointer("/checks/3/data/rootCause")
        , hsmStatusPointer("/checks/1/status")
        , hsmRootCausePointer("/checks/1/data/rootCause")
        , hsmIpPointer("/checks/1/data/ip")
        , redisStatusPointer("/checks/4/status")
        , redisRootCausePointer("/checks/4/data/rootCause")
        , tslStatusPointer("/checks/7/status")
        , tslRootCausePointer("/checks/7/data/rootCause")
        , tslExpiryDatePointer("/checks/7/data/expiryDate")
        , tslSequenceNumberPointer("/checks/7/data/sequenceNumber")
        , tslIdPointer("/checks/7/data/id")
        , tslHashPointer("/checks/7/data/hash")
        , bnaStatusPointer("/checks/0/status")
        , bnaRootCausePointer("/checks/0/data/rootCause")
        , bnaExpiryDatePointer("/checks/0/data/expiryDate")
        , bnaSequenceNumberPointer("/checks/0/data/sequenceNumber")
        , bnaIdPointer("/checks/0/data/id")
        , bnaHashPointer("/checks/0/data/hash")
        , idpStatusPointer("/checks/2/status")
        , idpRootCausePointer("/checks/2/data/rootCause")
        , seedTimerStatusPointer("/checks/5/status")
        , seedTimerRootCausePointer("/checks/5/data/rootCause")
        , teeTokenUpdaterStatusPointer("/checks/6/status")
        , teeTokenUpdaterRootCausePointer("/checks/6/data/rootCause")
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
        // prevent CFdSigErpManager from getting an OCSP-Response on initial request:
        HealthHandlerTestTslManager::failOcspRetrieval = true;
        createServiceContext();
    }

    void TearDown() override
    {
        HealthHandlerTestTslManager::failOcspRetrieval = false;
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

        factories.redisClientFactory = [](std::chrono::milliseconds){return std::make_unique<HealthHandlerTestMockRedisStore>();};
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
    rapidjson::Pointer currentTimestampPointer;
    rapidjson::Pointer postgresStatusPointer;
    rapidjson::Pointer postgresRootCausePointer;
    rapidjson::Pointer hsmStatusPointer;
    rapidjson::Pointer hsmRootCausePointer;
    rapidjson::Pointer hsmIpPointer;
    rapidjson::Pointer redisStatusPointer;
    rapidjson::Pointer redisRootCausePointer;
    rapidjson::Pointer tslStatusPointer;
    rapidjson::Pointer tslRootCausePointer;
    rapidjson::Pointer tslExpiryDatePointer;
    rapidjson::Pointer tslSequenceNumberPointer;
    rapidjson::Pointer tslIdPointer;
    rapidjson::Pointer tslHashPointer;
    rapidjson::Pointer bnaStatusPointer;
    rapidjson::Pointer bnaRootCausePointer;
    rapidjson::Pointer bnaExpiryDatePointer;
    rapidjson::Pointer bnaSequenceNumberPointer;
    rapidjson::Pointer bnaIdPointer;
    rapidjson::Pointer bnaHashPointer;
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
    // force refresh of OCSP-Response
    HealthHandlerTestTslManager::failOcspRetrieval = false;
    ASSERT_NO_THROW(mServiceContext->getCFdSigErpManager().getOcspResponseData(true));

    const auto beforeRequest = model::Timestamp::now();
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
    EXPECT_NO_THROW(
        model::Timestamp::fromXsDateTime(std::string(tslExpiryDatePointer.Get(healthDocument)->GetString())));
    EXPECT_FALSE(std::string(tslSequenceNumberPointer.Get(healthDocument)->GetString()).empty());
    EXPECT_FALSE(std::string(tslIdPointer.Get(healthDocument)->GetString()).empty());
    EXPECT_NO_THROW(ByteHelper::fromHex(std::string(tslHashPointer.Get(healthDocument)->GetString())));
    EXPECT_EQ(std::string(bnaStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_FALSE(std::string(bnaSequenceNumberPointer.Get(healthDocument)->GetString()).empty());
    EXPECT_NO_THROW(
        model::Timestamp::fromXsDateTime(std::string(bnaExpiryDatePointer.Get(healthDocument)->GetString())));
    EXPECT_FALSE(std::string(bnaIdPointer.Get(healthDocument)->GetString()).empty());
    EXPECT_NO_THROW(ByteHelper::fromHex(std::string(bnaHashPointer.Get(healthDocument)->GetString())));
    EXPECT_EQ(std::string(idpStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(cFdSigErpPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_NE(std::string(cFdSigErpTimestampPointer.Get(healthDocument)->GetString()), "never successfully validated");
    EXPECT_EQ(std::string(cFdSigErpPolicyPointer.Get(healthDocument)->GetString()), "C.FD.OSIG");
    EXPECT_EQ(std::string(cFdSigErpExpiryPointer.Get(healthDocument)->GetString()), "2025-12-31T23:00:00.000+00:00");
    EXPECT_EQ(std::string(seedTimerStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(teeTokenUpdaterStatusPointer.Get(healthDocument)->GetString()),
              std::string(model::Health::up));
    EXPECT_EQ(std::string(ErpServerInfo::BuildVersion()), std::string(buildPointer.Get(healthDocument)->GetString()));
    EXPECT_EQ(std::string(ErpServerInfo::BuildType()), std::string(buildTypePointer.Get(healthDocument)->GetString()));
    EXPECT_EQ(std::string(ErpServerInfo::ReleaseVersion()), std::string(releasePointer.Get(healthDocument)->GetString()));
    EXPECT_EQ(std::string(ErpServerInfo::ReleaseDate()), std::string(releasedatePointer.Get(healthDocument)->GetString()));

    std::optional<model::Timestamp> requestTimestamp;
    auto afterRequest = model::Timestamp::now();
    EXPECT_NO_THROW(requestTimestamp = model::Timestamp::fromXsDateTime(
                        std::string(currentTimestampPointer.Get(healthDocument)->GetString())));
    EXPECT_LE(beforeRequest, requestTimestamp);
    EXPECT_GE(afterRequest, requestTimestamp);
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
    verifyRootCause(healthDocument, tslRootCausePointer, "No TSL loaded");
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
    verifyRootCause(healthDocument, bnaRootCausePointer, "No BNetzA loaded");
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
    for (const auto& item : magic_enum::enum_values<ApplicationHealth::Service>())
    {
        if (item == ApplicationHealth::Service::EventDb)
        {
            continue;
        }
        mServiceContext->applicationHealth().enableCheck(item);
    }
    ASSERT_NO_THROW(handleRequest());

    ASSERT_EQ(mContext->response.getHeader().status(), HttpStatus::OK);

    rapidjson::Document healthDocument;
    auto body = mContext->response.getBody();
    healthDocument.Parse(body);

    // sub-status DOWN does not cause the application health to go down.
    EXPECT_EQ(std::string(statusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(cFdSigErpPointer.Get(healthDocument)->GetString()), std::string(model::Health::down));
    EXPECT_NE(std::string(cFdSigErpTimestampPointer.Get(healthDocument)->GetString()), "never successfully validated");
    EXPECT_EQ(std::string(cFdSigErpPolicyPointer.Get(healthDocument)->GetString()), "C.FD.OSIG");
    EXPECT_EQ(std::string(cFdSigErpExpiryPointer.Get(healthDocument)->GetString()), "2025-12-31T23:00:00.000+00:00");
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
    for (const auto& item : magic_enum::enum_values<ApplicationHealth::Service>())
    {
        if (item == ApplicationHealth::Service::EventDb)
        {
            continue;
        }
        mServiceContext->applicationHealth().enableCheck(item);
    }

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
        "std::runtime_error(ExceptionWrapper<std::runtime_error>)(no successful validation available) at ");

    EXPECT_TRUE(mContext->serviceContext.registrationInterface()->registered());
}
