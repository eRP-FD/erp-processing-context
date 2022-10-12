#include "erp/model/Consent.hxx"
#include "erp/model/Timestamp.hxx"
#include "test/erp/database/DatabaseTestFixture.hxx"

class ConsentTableTest : public DatabaseTestFixture
{
public:
    static constexpr std::string_view kvnr1{"X123456789"};
    static constexpr std::string_view kvnr2{"X012345678"};



    void SetUp() override
    {
        DatabaseTestFixture::SetUp();
        auto db = database();
        (void)db.clearConsent(kvnr1);
        (void)db.clearConsent(kvnr2);
        db.commitTransaction();
    }
};

TEST_F(ConsentTableTest, createAndGet)
{
    auto timestamp = model::Timestamp::fromFhirDateTime("2021-10-03");
    { // scope
        auto db = database();
        db.storeConsent(model::Consent{kvnr1, timestamp});
        db.commitTransaction();
    }
    { //scope
        auto db = database();
        auto fromDB = db.retrieveConsent(kvnr1);
        ASSERT_TRUE(fromDB.has_value());
        EXPECT_EQ(fromDB->id(), model::Consent::createIdString(model::Consent::Type::CHARGCONS, kvnr1));
        EXPECT_EQ(fromDB->patientKvnr(), kvnr1);
        EXPECT_TRUE(fromDB->isChargingConsent());
        EXPECT_EQ(fromDB->dateTime(), timestamp);
        db.commitTransaction();
    }
}

TEST_F(ConsentTableTest, notFound)
{
    { // scope
        auto db = database();
        auto fromDB1 = db.retrieveConsent(kvnr1);
        ASSERT_FALSE(fromDB1.has_value());
        auto fromDB2 = db.retrieveConsent(kvnr2);
        ASSERT_FALSE(fromDB2.has_value());
        db.commitTransaction();
    }
    { //scope
        auto db = database();
        auto timestamp = model::Timestamp::fromFhirDateTime("2021-10-03");
        db.storeConsent(model::Consent{kvnr1, timestamp});
        db.commitTransaction();
    }
    { // scope
        auto db = database();
        auto fromDB1 = db.retrieveConsent(kvnr1);
        ASSERT_TRUE(fromDB1.has_value());
        auto fromDB2 = db.retrieveConsent(kvnr2);
        ASSERT_FALSE(fromDB2.has_value());
        db.commitTransaction();
    }
}

TEST_F(ConsentTableTest, clear)
{
    { // scope
        auto db = database();
        auto timestamp = model::Timestamp::fromFhirDateTime("2021-10-03");
        EXPECT_FALSE(db.clearConsent(kvnr1));
        db.storeConsent(model::Consent{kvnr1, timestamp});
        db.commitTransaction();
    }
    { // scope
        auto db = database();
        auto fromDB1 = db.retrieveConsent(kvnr1);
        ASSERT_TRUE(fromDB1.has_value());
        db.commitTransaction();
    }
    { // scope
        auto db = database();
        EXPECT_TRUE(db.clearConsent(kvnr1));
        db.commitTransaction();
    }
    { // scope
        auto db = database();
        auto fromDB1 = db.retrieveConsent(kvnr1);
        ASSERT_FALSE(fromDB1.has_value());
        db.commitTransaction();
    }
}

TEST_F(ConsentTableTest, duplicate)//NOLINT(readability-function-cognitive-complexity)
{
    auto timestamp = model::Timestamp::fromFhirDateTime("2021-10-03");
    { // scope
        auto db = database();
        EXPECT_NO_THROW(db.storeConsent(model::Consent{kvnr1, timestamp}));
        db.commitTransaction();
    }
    { // scope
        auto db = database();
        EXPECT_THROW(db.storeConsent(model::Consent{kvnr1, timestamp}), pqxx::unique_violation);
        db.commitTransaction();
    }
}

