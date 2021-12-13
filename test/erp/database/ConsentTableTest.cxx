#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/Timestamp.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/util/TestConfiguration.hxx"

#include <gtest/gtest.h>

class ConsentTableTest : public ::testing::Test
{
public:
    static constexpr std::string_view kvnr1{"X123456789"};
    static constexpr std::string_view kvnr2{"X012345678"};



    void SetUp() override
    {
        if (usePostgres)
        {// only mock implemented for now
            GTEST_SKIP();
        }
        auto db = database();
        (void)db.clearConsent(kvnr1);
        (void)db.clearConsent(kvnr2);
    }

protected:
    DatabaseFrontend database()
    {
        if (usePostgres)
        {
            return DatabaseFrontend(std::make_unique<PostgresBackend>(), hsmPool, keyDerivation);
        }
        return DatabaseFrontend(std::make_unique<MockDatabaseProxy>(mockDatabase), hsmPool, keyDerivation);
    }

    const TestConfiguration& testConfiguration = TestConfiguration::instance();
    bool usePostgres = testConfiguration.getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false);

private:
    HsmPool hsmPool{
        std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(),
                                         MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory()};
    KeyDerivation keyDerivation{hsmPool};
    MockDatabase mockDatabase{hsmPool};
};

TEST_F(ConsentTableTest, createAndGet)
{
    auto timestamp = model::Timestamp::fromFhirDateTime("2021-10-03");
    database().storeConsent(model::Consent{kvnr1, timestamp});

    auto fromDB = database().getConsent(kvnr1);
    ASSERT_TRUE(fromDB.has_value());
    EXPECT_EQ(fromDB->id(), model::Consent::createIdString(model::Consent::Type::CHARGCONS, kvnr1));
    EXPECT_EQ(fromDB->patientKvnr(), kvnr1);
    EXPECT_TRUE(fromDB->isChargingConsent());
    EXPECT_EQ(fromDB->dateTime(), timestamp);
}

TEST_F(ConsentTableTest, notFound)
{
    {
        auto fromDB1 = database().getConsent(kvnr1);
        ASSERT_FALSE(fromDB1.has_value());
        auto fromDB2 = database().getConsent(kvnr2);
        ASSERT_FALSE(fromDB2.has_value());
    }
    auto timestamp = model::Timestamp::fromFhirDateTime("2021-10-03");
    database().storeConsent(model::Consent{kvnr1, timestamp});
    {
        auto fromDB1 = database().getConsent(kvnr1);
        ASSERT_TRUE(fromDB1.has_value());
        auto fromDB2 = database().getConsent(kvnr2);
        ASSERT_FALSE(fromDB2.has_value());
    }
}

TEST_F(ConsentTableTest, clear)
{
    auto timestamp = model::Timestamp::fromFhirDateTime("2021-10-03");
    EXPECT_FALSE(database().clearConsent(kvnr1));
    database().storeConsent(model::Consent{kvnr1, timestamp});
    {
        auto fromDB1 = database().getConsent(kvnr1);
        ASSERT_TRUE(fromDB1.has_value());
    }
    EXPECT_TRUE(database().clearConsent(kvnr1));
    {
        auto fromDB1 = database().getConsent(kvnr1);
        ASSERT_FALSE(fromDB1.has_value());
    }
}

TEST_F(ConsentTableTest, duplicate)
{
    auto timestamp = model::Timestamp::fromFhirDateTime("2021-10-03");

    EXPECT_NO_THROW(database().storeConsent(model::Consent{kvnr1, timestamp}));
    EXPECT_THROW(database().storeConsent(model::Consent{kvnr1, timestamp}), pqxx::unique_violation);
}

