/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */
#include <gtest/gtest.h>

#include "erp/pc/pre_user_pseudonym/PreUserPseudonymManager.hxx"

#include "erp/database/DatabaseFrontend.hxx"
#include "erp/model/Task.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/util/Expect.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/util/StaticData.hxx"
#include "test/mock/RegistrationMock.hxx"
#include "test/util/ResourceManager.hxx"


static auto todaysKey = CmacKey::fromBin("--Today's Key!--"); // NOLINT
static auto yesterdaysKey = CmacKey::fromBin("Yesterday's Key!"); // NOLINT
static auto wrongKey = CmacKey::fromBin("-The Wrong Key!-"); // NOLINT

class PreUserPseudonymCmacTestDatabase : public MockDatabase
{
    using MockDatabase::MockDatabase;

    CmacKey acquireCmac(const date::sys_days& validDate, const CmacKeyCategory& category, RandomSource&) override
    {
        if (category != CmacKeyCategory::user)
        {
            return wrongKey;
        }
        if (fail_)
        {
            throw std::runtime_error("failed");
        }
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

public:
    static bool fail_;
};
bool PreUserPseudonymCmacTestDatabase::fail_ = false;

class PreUserPseudonymCmacTest : public ::testing::Test
{
public:
    void SetUp() override
    {
        tslEnvironmentGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der"));
        auto factories = StaticData::makeMockFactories();
        factories.databaseFactory = [](HsmPool& hsmPool, KeyDerivation& keyDerivation) {
            return std::make_unique<DatabaseFrontend>(std::make_unique<PreUserPseudonymCmacTestDatabase>(hsmPool),
                                                      hsmPool, keyDerivation);
        };
        serviceContext = std::make_unique<PcServiceContext>(Configuration::instance(), std::move(factories));
    }

    void TearDown() override
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
    auto resToday     = preUserPseudonymCmac.verifyAndReSign(sigToday    , testSub);
    auto resYesterday = preUserPseudonymCmac.verifyAndReSign(sigYesterday, testSub);
    auto resWrong     = preUserPseudonymCmac.verifyAndReSign(sigWrong    , testSub);
    EXPECT_TRUE (get<0>(resToday    ));
    EXPECT_TRUE (get<0>(resYesterday));
    EXPECT_FALSE(get<0>(resWrong    ));
    EXPECT_EQ(get<1>(resToday)    , sigToday);
    EXPECT_EQ(get<1>(resYesterday), sigToday);
    EXPECT_EQ(get<1>(resWrong)    , sigToday);
}

// regression test for ERP-6504 Pre-User-Pseudonym CMAC-Key not loaded when first load fails
TEST_F(PreUserPseudonymCmacTest, ERP6504RecoverAfterFailure)//NOLINT(readability-function-cognitive-complexity)
{
    std::string_view testSub{"RabcUSuuWKKZEEHmrcNm_kUDOW13uaGU5Zk8OoBwiNk"};
    PreUserPseudonymCmacTestDatabase::fail_ = true;
    getPreUserPseudonymCmac().mKeyDate = {};
    ASSERT_ANY_THROW(getPreUserPseudonymCmac().LoadCmacs(
        std::chrono::time_point_cast<date::days>(std::chrono::system_clock::now())));
    ASSERT_ANY_THROW((void) getPreUserPseudonymCmac().sign(testSub));

    PreUserPseudonymCmacTestDatabase::fail_ = false;
    ASSERT_NO_THROW((void) getPreUserPseudonymCmac().sign(testSub));
}
