#include "erp/pc/pre_user_pseudonym/PreUserPseudonymManager.hxx"

#include "erp/database/DatabaseFrontend.hxx"
#include "erp/model/Task.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/context/ServiceContext.hxx"
#include "erp/service/DosHandler.hxx"
#include "erp/util/Expect.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobDatabase.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>


static auto todaysKey = CmacKey::fromBin("--Today's Key!--"); // NOLINT
static auto yesterdaysKey = CmacKey::fromBin("Yesterday's Key!"); // NOLINT
static auto wrongKey = CmacKey::fromBin("-The Wrong Key!-"); // NOLINT

class PreUserPseudonymCmacTestDatabase : public MockDatabase
{
    using MockDatabase::MockDatabase;

    CmacKey acquireCmac(const date::sys_days& validDate, RandomSource&) override
    {
        auto today = std::chrono::time_point_cast<date::days>(std::chrono::system_clock::now());
        if (validDate == today)
        {
            return todaysKey;
        }
        else
        {
            Expect(validDate + date::days{1} == today, "acquireCmac called with unexpected date");
            return yesterdaysKey;
        }
    }
};

class PreUserPseudonymCmacTest : public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        tslEnvironmentGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            std::string{TEST_DATA_DIR} + "/tsl/TslSignerCertificateIssuer.der");
        serviceContext = std::make_unique<PcServiceContext>(Configuration::instance(),
            [] (HsmPool& hsmPool, KeyDerivation& keyDerivation){
                    return std::make_unique<DatabaseFrontend>(
                                std::make_unique<PreUserPseudonymCmacTestDatabase>(hsmPool), hsmPool, keyDerivation);
                },
            std::make_unique<DosHandler>(std::make_unique<MockRedisStore>()),
            std::make_unique<HsmPool>(
                std::make_unique<HsmMockFactory>(
                    std::make_unique<HsmMockClient>(),
                    MockBlobCache::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
                TeeTokenUpdater::createMockTeeTokenUpdaterFactory()),
            StaticData::getJsonValidator(),
            StaticData::getXmlValidator(),
            TslTestHelper::createTslManager<TslManager>());
    }

    virtual void TearDown() override
    {
        tslEnvironmentGuard.reset();
    }

    PreUserPseudonymManager& getPreUserPseudonymCmac()
    {
        return serviceContext->getPreUserPseudonymManager();
    }

private:
    std::unique_ptr<PcServiceContext> serviceContext;
    std::unique_ptr<EnvironmentVariableGuard> tslEnvironmentGuard;
};


TEST_F(PreUserPseudonymCmacTest, sign) // NOLINT
{
    std::string_view testSub{"RabcUSuuWKKZEEHmrcNm_kUDOW13uaGU5Zk8OoBwiNk"};
    auto expected = todaysKey.sign(testSub);
    auto actual = getPreUserPseudonymCmac().sign(testSub);
    EXPECT_EQ(actual, expected);
}

TEST_F(PreUserPseudonymCmacTest, verifyAndResign) // NOLINT
{
    using std::get;
    auto& preUserPseudonymCmac = getPreUserPseudonymCmac();
    std::string_view testSub{"RabcUSuuWKKZEEHmrcNm_kUDOW13uaGU5Zk8OoBwiNk"};
    auto sigToday     = todaysKey.sign(testSub);
    auto sigYesterday = yesterdaysKey.sign(testSub);
    auto sigWrong     = wrongKey.sign(testSub);
    auto resToday     = preUserPseudonymCmac.verifyAndResign(sigToday    , testSub);
    auto resYesterday = preUserPseudonymCmac.verifyAndResign(sigYesterday, testSub);
    auto resWrong     = preUserPseudonymCmac.verifyAndResign(sigWrong    , testSub);
    EXPECT_TRUE (get<0>(resToday    ));
    EXPECT_TRUE (get<0>(resYesterday));
    EXPECT_FALSE(get<0>(resWrong    ));
    EXPECT_EQ(get<1>(resToday)    , sigToday);
    EXPECT_EQ(get<1>(resYesterday), sigToday);
    EXPECT_EQ(get<1>(resWrong)    , sigToday);
}
