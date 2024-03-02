/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/erp/database/PostgresDatabaseTest.hxx"
#include "erp/crypto/CMAC.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/pc/telematic_pseudonym/TelematicPseudonymManager.hxx"
#include "erp/util/search/SearchParameter.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockRandom.hxx"
#include "test/mock/RegistrationMock.hxx"


using namespace model;

PostgresDatabaseTest::PostgresDatabaseTest() :
    mIdpPrivateKey(MockCryptography::getIdpPrivateKey()),
    mJwtBuilder(mIdpPrivateKey),
    mPharmacy(mJwtBuilder.makeJwtApotheke().stringForClaim(JWT::idNumberClaim).value()),
    mConnection(nullptr),
    mDatabase(nullptr),
    mBlobCache(nullptr),
    mHsmPool(nullptr),
    mKeyDerivation(nullptr),
    mDurationConsumerGuard(nullptr)
{
    mBlobCache = MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm);

    auto blobCache = mBlobCache;
    mHsmPool = std::make_unique<HsmPool>(
        std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(), std::move(blobCache)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>());
    mKeyDerivation = std::make_unique<KeyDerivation>(*mHsmPool);
    mDurationConsumerGuard = std::make_unique<DurationConsumerGuard>("PostgresDatabaseTest");
}


TEST_F(PostgresDatabaseTest, acquireCMACCommits)//NOLINT(readability-function-cognitive-complexity)
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

        ASSERT_NO_THROW( cmac1st = db.acquireCmac(today, CmacKeyCategory::user, mockRng));
        db.commitTransaction();
        ASSERT_TRUE( cmac1st );
        {
            auto&& txn = createTransaction();
            pqxx::result result;
            ASSERT_NO_THROW(result = txn.exec_prepared(retrieveCmac.name, todayStr));
            ASSERT_FALSE(result.empty());
            EXPECT_EQ(result.size(), 1);
            auto CmacDirectDB = result.front().at(0).as<db_model::postgres_bytea>();
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
        ASSERT_NO_THROW(cmac2nd = db.acquireCmac(today, CmacKeyCategory::user, mockRng));
        db.commitTransaction();
        ASSERT_TRUE(cmac2nd);
        EXPECT_EQ(toHex(*cmac2nd), toHex(*cmac1st));
    }
}

TEST_F(PostgresDatabaseTest, swapCMAC)//NOLINT(readability-function-cognitive-complexity)
{
    if (! usePostgres())
    {
        GTEST_SKIP();
    }

    const std::string source = "secret information";

    std::unique_ptr<PcServiceContext> serviceContext = std::make_unique<PcServiceContext>(
        Configuration::instance(), StaticData::makeMockFactories());

    //
    // Create instance, note that the constructor creates two cmac keys. The first key is for the quarter which contains todays date and
    // the second is for the upcoming quarter.
    //
    // Cmac Key 1:   [quarterBegin ... today ... quarterEnd]
    // Cmac Key 2:   [nextQuarterBegin ... nextQuarterEnd]
    // Grace Period: starts from 00:00:00 of last days quarterEndd and ends at 24:00:00 of first days nextQuarterBegin.
    //
    // Within the grace period, both keys are accepted. Expired keys are left in the table and are not considered since the
    // TelematicPseudonymManager always works with the current date.
    //
    std::unique_ptr<TelematicPseudonymManager> mTelematicPseudonymManager =
        TelematicPseudonymManager::create(serviceContext.get());

    // Empty the keys table in order to not interfere with previous keys and have a clean database state.
    {// scope
        auto deleteTxn = createTransaction();
        deleteTxn.exec("DELETE FROM erp.vau_cmac");
        deleteTxn.commit();
    }

    const Query retrieveCmac{"retrieveCmac", "SELECT * FROM erp.vau_cmac WHERE cmac_type = 'telematic'"};
    prepare(retrieveCmac);

    {
        // Set todays date and create keys.
        auto today = date::year_month_day(date::year{1997}, date::month{10}, date::day{01});
        mTelematicPseudonymManager->LoadCmacs(today);

        {// scope
            pqxx::result result;
            auto&& txn = createTransaction();
            ASSERT_NO_THROW(result = txn.exec_prepared(retrieveCmac.name));
            txn.commit();
            EXPECT_EQ(result.size(), 2);
        }

        // Test within first quarter.
        auto queryDate = date::year_month_day(date::year{1997}, date::month{10}, date::day{01});
        EXPECT_FALSE(mTelematicPseudonymManager->keyUpdateRequired(queryDate));
        EXPECT_FALSE(mTelematicPseudonymManager->withinGracePeriod(queryDate));

        // Test at end of first quarter (key1, grace period.)
        queryDate = date::year_month_day(date::year{1997}, date::month{12}, date::day{31});
        EXPECT_FALSE(mTelematicPseudonymManager->keyUpdateRequired(queryDate));
        EXPECT_TRUE(mTelematicPseudonymManager->withinGracePeriod(queryDate));

        // Test at beginning of next quarter (key2, still in grace period.)
        queryDate = date::year_month_day(date::year{1998}, date::month{01}, date::day{01});
        EXPECT_TRUE(mTelematicPseudonymManager->keyUpdateRequired(queryDate));
        EXPECT_TRUE(mTelematicPseudonymManager->withinGracePeriod(queryDate));
    }

    // Sign with the key expected to be used in the future, when the first quarter is over.
    auto signed_with_key1 = mTelematicPseudonymManager->sign(1, source);

    {
        // Assume this date now as "today".
        auto today = date::year_month_day(date::year{1998}, date::month{01}, date::day{01});
        // Update the keys. Former cmac key 2 is now cmac key 1 and a new cmac key 2 is created.
        // Former cmac key 1 remains unused in the table.
        mTelematicPseudonymManager->LoadCmacs(today);
        {// scope
            pqxx::result result;
            auto&& txn = createTransaction();
            ASSERT_NO_THROW(result = txn.exec_prepared(retrieveCmac.name));
            txn.commit();
            EXPECT_EQ(result.size(), 3);
        }

        auto queryDate = date::year_month_day(date::year{1998}, date::month{01}, date::day{01});
        EXPECT_FALSE(mTelematicPseudonymManager->keyUpdateRequired(queryDate));
        EXPECT_FALSE(mTelematicPseudonymManager->withinGracePeriod(queryDate));

        auto signed_with_key0_which_was_key1 = mTelematicPseudonymManager->sign(0, source);
        EXPECT_EQ(signed_with_key0_which_was_key1.hex(), signed_with_key1.hex());
    }
}

TEST_F(PostgresDatabaseTest, countAllTasksForPatient)
{
    if (! usePostgres())
    {
        GTEST_SKIP();
    }
    auto allTypes = magic_enum::enum_values<model::PrescriptionType>();
    const model::Kvnr kvnr{"X012345676", model::Kvnr::Type::gkv};
    SearchParameter paramStatus{"status", "status", SearchParameter::Type::TaskStatus};
    auto taskCount = [&](const std::optional<UrlArguments>& search = std::nullopt) {
        auto count = database().countAllTasksForPatient(kvnr, search);
        database().commitTransaction();
        return count;
    };
    auto kvnrHashed = getKeyDerivation().hashKvnr(kvnr);
    for (auto prescType : allTypes)
    {
        auto txn = createTransaction();
        txn.exec_params("DELETE FROM " + taskTableName(prescType) + " WHERE kvnr_hashed = $1",
                        kvnrHashed.binarystring());
        txn.commit();
    }
    ASSERT_EQ(taskCount(), 0);
    std::list<model::Task> tasks;
    for (auto prescType : allTypes)
    {
        auto& task = tasks.emplace_back(prescType, "access_code");
        task.setPrescriptionId(database().storeTask(task));
        task.setStatus(model::Task::Status::ready);
        task.setKvnr(kvnr);
        task.setAcceptDate(model::Timestamp::now());
        task.setExpiryDate(model::Timestamp::now() + std::chrono::days(30));
        auto prescriptionDummy = model::Binary(Uuid().toString(), "{}");
        database().activateTask(task, prescriptionDummy);
    }
    database().commitTransaction();
    EXPECT_EQ(taskCount(), 4);
    UrlArguments ready{{paramStatus}};
    ready.parse({{"status", "ready"}}, getKeyDerivation());
    EXPECT_EQ(taskCount(ready), 4);
    auto& task = tasks.front();
    task.setStatus(model::Task::Status::inprogress);
    task.setSecret("secret");
    database().updateTaskStatusAndSecret(task);
    EXPECT_EQ(taskCount(), 4);
    EXPECT_EQ(taskCount(ready), 3);
}
