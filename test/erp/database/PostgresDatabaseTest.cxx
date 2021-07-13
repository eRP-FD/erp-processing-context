#include "test/erp/database/PostgresDatabaseTest.hxx"


#include "erp/crypto/CMAC.hxx"
#include "erp/hsm/BlobCache.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/hsm/MockBlobDatabase.hxx"
#include "test/mock/MockRandom.hxx"


using namespace model;

PostgresDatabaseTest::PostgresDatabaseTest() :
    mIdpPrivateKey(MockCryptography::getIdpPrivateKey()),
    mJwtBuilder(mIdpPrivateKey),
    mPharmacy(mJwtBuilder.makeJwtApotheke().stringForClaim(JWT::idNumberClaim).value()),
    mConnection(nullptr),
    mDatabase(nullptr),
    mBlobCache(nullptr),
    mHsmPool(nullptr),
    mKeyDerivation(nullptr)
{
    mBlobCache = MockBlobCache::createBlobCache(MockBlobCache::MockTarget::MockedHsm);

    auto blobCache = mBlobCache;
    mHsmPool = std::make_unique<HsmPool>(
        std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(), std::move(blobCache)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory());
    mKeyDerivation = std::make_unique<KeyDerivation>(*mHsmPool);
}


TEST_F(PostgresDatabaseTest, acquireCMACCommits)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }
    { // scope
        auto deleteTxn = createTransaction();
        deleteTxn.exec("DELETE FROM erp.vau_cmac");
        deleteTxn.commit();
    }
    std::ostringstream todayStrm;
    auto today = std::chrono::time_point_cast<date::days>(std::chrono::system_clock::now());
    todayStrm << date::year_month_day{today};
    auto todayStr = todayStrm.str();

    const Query retrieveCmac{"retrieveCmac", "SELECT cmac FROM erp.vau_cmac WHERE valid_date = $1"};
    prepare(retrieveCmac);
    { // scope
        pqxx::result result;
        auto&& txn = createTransaction();
        ASSERT_NO_THROW(result = txn.exec_prepared(retrieveCmac.name, todayStr));
        ASSERT_TRUE(result.empty());
        txn.commit();
    }
    MockRandom mockRng;
    // test with 1st DB instance
    // should create entry in DB
    std::optional<CmacKey> cmac1st;
    { // scope
        PostgresBackend db;

        mockRng.SetRandomBytesOffset(0);

        ASSERT_NO_THROW( cmac1st = db.acquireCmac(today, mockRng));
        db.commitTransaction();
        ASSERT_TRUE( cmac1st );
        {
            auto&& txn = createTransaction();
            pqxx::result result;
            ASSERT_NO_THROW(result = txn.exec_prepared(retrieveCmac.name, todayStr));
            ASSERT_FALSE(result.empty());
            EXPECT_EQ(result.size(), 1);
            auto CmacDirectDB = result.front().at(0).as<pqxx::binarystring>();
            EXPECT_EQ(toHex(*cmac1st), toHex(CmacDirectDB));
            txn.commit();
        }
    }
    // test with second DB instance
    // should read existing entry
    { // scope
        PostgresBackend db;

        mockRng.SetRandomBytesOffset(1);

        std::optional<CmacKey> cmac2nd;
        ASSERT_NO_THROW(cmac2nd = db.acquireCmac(today, mockRng));
        db.commitTransaction();
        ASSERT_TRUE(cmac2nd);
        EXPECT_EQ(toHex(*cmac2nd), toHex(*cmac1st));
    }
}
