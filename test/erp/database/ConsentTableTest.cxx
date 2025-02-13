#include "erp/model/Consent.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/model/Timestamp.hxx"
#include "test/erp/database/DatabaseTestFixture.hxx"
#include "test/util/TestUtils.hxx"

class ConsentTableTest : public DatabaseTestFixture<>
{
public:
    const model::Kvnr kvnr1{"X123456788", model::Kvnr::Type::pkv};
    const model::Kvnr kvnr2{"X012345676", model::Kvnr::Type::pkv};

    void SetUp() override
    {
        DatabaseTestFixture::SetUp();
        auto db = database();
        (void)db.clearConsent(kvnr1);
        (void)db.clearConsent(kvnr2);
        db.commitTransaction();
    }

    void TearDown() override
    {
        DatabaseTestFixture::TearDown();
    }
};

TEST_F(ConsentTableTest, createAndGet)
{
    auto startTime = model::Timestamp::now();
    auto timestamp = model::Timestamp::fromFhirDateTime("2021-10-03");
    { // scope
        auto db = database();
        db.storeConsent(model::Consent{kvnr1, timestamp});
        db.commitTransaction();
    }
    auto endTime = model::Timestamp::now();
    { //scope
        auto db = database();
        auto fromDB = db.retrieveConsent(kvnr1);
        ASSERT_TRUE(fromDB.has_value());
        EXPECT_EQ(fromDB->id(), model::Consent::createIdString(model::Consent::Type::CHARGCONS, kvnr1));
        EXPECT_EQ(fromDB->patientKvnr(), kvnr1);
        EXPECT_TRUE(fromDB->isChargingConsent());
        EXPECT_GE(fromDB->dateTime().toChronoTimePoint(), startTime.toChronoTimePoint());
        EXPECT_LE(fromDB->dateTime().toChronoTimePoint(), endTime.toChronoTimePoint());
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

// GEMREQ-start A_22158
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
// GEMREQ-end A_22158

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
