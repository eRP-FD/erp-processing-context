/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

// NOLINTBEGIN
#include <gtest/gtest.h>
// NOLINTEND
#include "erp/ErpRequirements.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/pc/telematic_pseudonym/TelematicPseudonymManager.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/mock/RegistrationMock.hxx"
#include "test/util/ResourceManager.hxx"

#include <shared_mutex>


// Length must be 16
static auto wrongKey = CmacKey::fromBin("-The Wrong Key!-");
static auto k_today = CmacKey::fromBin("0000000000000000");
static auto k_1997_12_31 = CmacKey::fromBin("0123456789AB0000");
static auto k_1998_01_01 = CmacKey::fromBin("0123456789AB0001");

class TelematicPseudonymCmacTestDatabase : public MockDatabase
{
    using MockDatabase::MockDatabase;

    CmacKey acquireCmac(const date::sys_days& validDate, const CmacKeyCategory& category, RandomSource&) override
    {
        // Use prepared keys for the past, otherwise return the same key for today.
        const auto tNow = std::chrono::system_clock::now();
        const auto today = date::year_month_day(std::chrono::time_point_cast<date::days>(tNow));
        if (date::sys_days{today} == validDate)
        {
            return k_today;
        }
        if (category == CmacKeyCategory::telematic)
        {
            using namespace date;
            if (validDate == year_month_day(day{31} / month{12} / year{1997}))
                return k_1997_12_31;
            if (validDate == year_month_day(day{1} / month{1} / year{1998}))
                return k_1998_01_01;
        }
        return k_today;
    }

public:
    static bool fail_;
};
bool TelematicPseudonymCmacTestDatabase::fail_ = false;

class TelematicPseudonymCmacTest : public ::testing::Test
{
public:
    void SetUp() override
    {
        tslEnvironmentGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH", ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der"));
        auto factories = StaticData::makeMockFactories();
        factories.databaseFactory = [](HsmPool& hsmPool, KeyDerivation& keyDerivation) {
            return std::make_unique<DatabaseFrontend>(std::make_unique<TelematicPseudonymCmacTestDatabase>(hsmPool),
                                                      hsmPool, keyDerivation);
        };
        serviceContext = std::make_unique<PcServiceContext>(Configuration::instance(), std::move(factories));
    }

    void TearDown() override
    {
        tslEnvironmentGuard.reset();
    }

    TelematicPseudonymManager& getTelematicPseudonymManager()
    {
        return serviceContext->getTelematicPseudonymManager();
    }

private:
    std::unique_ptr<PcServiceContext> serviceContext;
    std::unique_ptr<EnvironmentVariableGuard> tslEnvironmentGuard;
};


TEST_F(TelematicPseudonymCmacTest, sign)
{
    std::string_view source{"RabcUSuuWKKZEEHmrcNm_kUDOW13uaGU5Zk8OoBwiNk"};
    auto expected = k_today.sign(source);
    auto actual = getTelematicPseudonymManager().sign(source);
    EXPECT_EQ(actual, expected);
}

TEST_F(TelematicPseudonymCmacTest, KeyUpdate)
{
    auto today = date::year_month_day(date::year{1997}, date::month{10}, date::day{1});
    getTelematicPseudonymManager().LoadCmacs(today);

    EXPECT_FALSE(getTelematicPseudonymManager().keyUpdateRequired(today));

    today = date::year_month_day(date::year{1997}, date::month{12}, date::day{30});
    EXPECT_FALSE(getTelematicPseudonymManager().keyUpdateRequired(today));

    today = date::year_month_day(date::year{1997}, date::month{12}, date::day{31});
    EXPECT_FALSE(getTelematicPseudonymManager().keyUpdateRequired(today));

    today = date::year_month_day(date::year{1998}, date::month{01}, date::day{01});
    EXPECT_TRUE(getTelematicPseudonymManager().keyUpdateRequired(today));

    today = date::year_month_day(date::year{1998}, date::month{02}, date::day{01});
    EXPECT_TRUE(getTelematicPseudonymManager().keyUpdateRequired(today));
}

TEST_F(TelematicPseudonymCmacTest, GracePeriod)
{
    auto today = date::year_month_day(date::year{1997}, date::month{10}, date::day{01});
    getTelematicPseudonymManager().LoadCmacs(today);
    EXPECT_FALSE(getTelematicPseudonymManager().withinGracePeriod(today));

    today = date::year_month_day(date::year{1997}, date::month{12}, date::day{31});
    EXPECT_FALSE(getTelematicPseudonymManager().withinGracePeriod(today));

    today = date::year_month_day(date::year{1998}, date::month{1}, date::day{1});
    EXPECT_TRUE(getTelematicPseudonymManager().withinGracePeriod(today));

    today = date::year_month_day(date::year{1998}, date::month{1}, date::day{2});
    EXPECT_FALSE(getTelematicPseudonymManager().withinGracePeriod(today));
}

TEST_F(TelematicPseudonymCmacTest, LoadCMAC)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace date;

    auto today = year_month_day(year{1997}, month{10}, day{01});
    getTelematicPseudonymManager().LoadCmacs(today);
    {
        const auto [key1Start, key1End] = getTelematicPseudonymManager().getKey1Range();
        const auto [key2Start, key2End] = getTelematicPseudonymManager().getKey2Range();
        EXPECT_EQ(key1Start, year_month_day(year{1997}, month{10}, day{1}));
        EXPECT_EQ(key1End, year_month_day(year{1997}, month{12}, day{31}));
        EXPECT_EQ(key2Start, year_month_day(year{1998}, month{1}, day{1}));
        EXPECT_EQ(key2End, year_month_day(year{1998}, month{3}, day{31}));
        today = key2Start;
    }
    {
        getTelematicPseudonymManager().LoadCmacs(today);
        const auto [key1Start, key1End] = getTelematicPseudonymManager().getKey1Range();
        const auto [key2Start, key2End] = getTelematicPseudonymManager().getKey2Range();
        EXPECT_EQ(key1Start, year_month_day(year{1998}, month{1}, day{1}));
        EXPECT_EQ(key1End, year_month_day(year{1998}, month{3}, day{31}));
        EXPECT_EQ(key2Start, year_month_day(year{1998}, month{4}, day{1}));
        EXPECT_EQ(key2End, year_month_day(year{1998}, month{6}, day{30}));
        today = key2Start;
    }
    {
        getTelematicPseudonymManager().LoadCmacs(today);
        const auto [key1Start, key1End] = getTelematicPseudonymManager().getKey1Range();
        const auto [key2Start, key2End] = getTelematicPseudonymManager().getKey2Range();
        EXPECT_EQ(key1Start, year_month_day(year{1998}, month{4}, day{1}));
        EXPECT_EQ(key1End, year_month_day(year{1998}, month{6}, day{30}));
        EXPECT_EQ(key2Start, year_month_day(year{1998}, month{7}, day{1}));
        EXPECT_EQ(key2End, year_month_day(year{1998}, month{9}, day{30}));
    }
}

TEST_F(TelematicPseudonymCmacTest, KeySwap)
{
    using namespace date;

    std::string source = "secret information";

    A_22383.test("Test for a key swap when three months are exceeded while trying to sign.");
    // Start at 1997-10-05
    auto today = year_month_day(year{1997}, month{10}, day{05});
    getTelematicPseudonymManager().LoadCmacs(today);
    auto sig0_k1 = getTelematicPseudonymManager().sign(0, source);

    // Next at last-10-1997, no key change:
    getTelematicPseudonymManager().ensureKeysUptodateForDay(year_month_day(last / 10 / 1997));
    auto sig1_k1 = getTelematicPseudonymManager().sign(0, source);
    auto sig1_k2 = getTelematicPseudonymManager().sign(1, source);
    EXPECT_EQ(sig0_k1.hex(), sig1_k1.hex());
    EXPECT_NE(sig0_k1.hex(), sig1_k2.hex());

    // Next at 1998-01-05, key change (previous key2 is now key1 and key2 is a new):
    getTelematicPseudonymManager().ensureKeysUptodateForDay(year_month_day(year{1998}, month{1}, day{5}));
    auto sig2_k1 = getTelematicPseudonymManager().sign(0, source);
    auto sig2_k2 = getTelematicPseudonymManager().sign(1, source);
    EXPECT_EQ(sig1_k2.hex(), sig2_k1.hex());
    EXPECT_NE(sig1_k2.hex(), sig2_k2.hex());
    A_22383.finish();
}
