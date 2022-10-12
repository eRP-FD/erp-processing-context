/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/BlobDatabase.hxx"
#include "erp/hsm/production/ProductionBlobDatabase.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/TLog.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/util/BlobDatabaseHelper.hxx"
#include "test/util/TestConfiguration.hxx"

#include <gtest/gtest.h>


namespace {
    using DatabaseFactory = std::function<std::unique_ptr<BlobDatabase>()>;

    // Provide two 0 to 1 element lists of database factories so that all tests can run
    // at least with the mock implementation and, if supported, as well with the production database.

    std::vector<std::tuple<DatabaseFactory,bool>> getProductionDatabaseFactories (void)
    {
        std::vector<std::tuple<DatabaseFactory,bool>> factories;
        if (TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
            factories.emplace_back([]{return std::make_unique<ProductionBlobDatabase>();}, false);
        return factories;
    }

    class MockBlobDatabaseWrapper : public BlobDatabase
    {
    public:
        explicit MockBlobDatabaseWrapper (MockBlobDatabase& database) : mDatabase(database) {}
        Entry getBlob (BlobType type, BlobId id) const override {return mDatabase.getBlob(type,id);}
        std::vector<Entry> getAllBlobsSortedById (void) const override {return mDatabase.getAllBlobsSortedById();}
        BlobId storeBlob (Entry&& entry) override {return mDatabase.storeBlob(std::move(entry));}
        void deleteBlob (BlobType type, const ErpVector& name) override {mDatabase.deleteBlob(type,name);}
        std::vector<bool> hasValidBlobsOfType (std::vector<BlobType>&& blobTypes) const override {return mDatabase.hasValidBlobsOfType(std::move(blobTypes));}
    private:
        MockBlobDatabase& mDatabase;
    };

    std::vector<std::tuple<DatabaseFactory,bool>> getMockDatabaseFactories (void)
    {
        std::vector<std::tuple<DatabaseFactory,bool>> factories;
        // The mock database is not persistent, so keep once instance alive during a whole test.
        factories.emplace_back(
            [instance=std::make_shared<MockBlobDatabase>()]
            ()mutable
            {
                return std::make_unique<MockBlobDatabaseWrapper>(*instance);
            },
            true);
        return factories;
    }
}

class BlobDatabaseTest : public testing::Test,
                         public testing::WithParamInterface<std::tuple<DatabaseFactory,bool>>
{
public:
    void SetUp (void) override
    {
        BlobDatabaseHelper::removeUnreferencedBlobs();
    }

    void TearDown (void) override
    {
        deleteAllItems();
    }

    void rememberToDeleteBlob (const BlobType type, const ErpVector& name)
    {
        mBlobsToDelete.emplace_back(type,name);
    }

    void markAsDeleted (const BlobType type, const ErpVector& name)
    {
        for (auto item=mBlobsToDelete.begin(); item!=mBlobsToDelete.end(); ++item)
        {
            if (std::get<0>(*item) == type
                && std::get<1>(*item) == name)
            {
                mBlobsToDelete.erase(item);
                return;
            }
        }
        FAIL();
    }

    std::chrono::system_clock::time_point nowPlus (const std::chrono::system_clock::duration offset)
    {
        return std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() + offset);
    }

private:
    using Item = std::tuple<BlobType,ErpVector>;
    std::vector<Item> mBlobsToDelete;

    void deleteAllItems (void)
    {
        auto blobDB = std::get<DatabaseFactory>(GetParam())();
        for (const auto& item : mBlobsToDelete)
            blobDB->deleteBlob(std::get<0>(item), std::get<1>(item));
    }
};


TEST_P(BlobDatabaseTest, blob_crud) // NOLINT(readability-function-cognitive-complexity)
{
    const auto& databaseFactory = std::get<0>(GetParam());

    // Verify that initially there is no EciesKeyPair|1 blob.
    ASSERT_ANY_THROW(
        databaseFactory()->getBlob(BlobType::EciesKeypair, 1));

    // Create the blob.
    BlobDatabase::Entry entry;
    entry.type = BlobType::EciesKeypair;
    entry.name = ErpVector::create("name is usually binary data");
    entry.blob.data = SafeString("this is the blob data");
    entry.blob.generation = 1;
    entry.expiryDateTime = nowPlus(std::chrono::minutes(1));
    entry.startDateTime = nowPlus(std::chrono::minutes(-1));
    entry.endDateTime = nowPlus(std::chrono::minutes(2));
    entry.id = 0;

    rememberToDeleteBlob(entry.type, entry.name);
    auto const blobId = databaseFactory()->storeBlob(BlobDatabase::Entry(entry)); // Pass in a copy so that we can use entry later on.
    ASSERT_GT(blobId, static_cast<BlobId>(0));

    // Now repeat the getBlob but this time expect that the blob is found.
    const auto retrievedEntry = databaseFactory()->getBlob(BlobType::EciesKeypair, blobId);
    EXPECT_EQ(retrievedEntry.type, entry.type);
    EXPECT_EQ(retrievedEntry.name, entry.name);
    EXPECT_EQ(retrievedEntry.blob.data, entry.blob.data);
    EXPECT_EQ(retrievedEntry.blob.generation, entry.blob.generation);
    EXPECT_EQ(retrievedEntry.id, blobId);
    EXPECT_EQ(retrievedEntry.expiryDateTime, entry.expiryDateTime);
    EXPECT_EQ(retrievedEntry.startDateTime, entry.startDateTime);
    EXPECT_EQ(retrievedEntry.endDateTime, entry.endDateTime);

    // Finally delete the blob.
    databaseFactory()->deleteBlob(entry.type, entry.name);
    markAsDeleted(entry.type, entry.name);
}


TEST_P(BlobDatabaseTest, storeBlob_failForDuplicateName) // NOLINT(readability-function-cognitive-complexity)
{
    const auto& databaseFactory = std::get<0>(GetParam());

    // Create the blob.
    BlobDatabase::Entry entry;
    entry.type = BlobType::EciesKeypair;
    entry.name = ErpVector::create("name is usually binary data");
    entry.blob.data = SafeString("this is the blob data");
    entry.blob.generation = 1;
    entry.id = 0;

    rememberToDeleteBlob(entry.type, entry.name);
    auto const blobId = databaseFactory()->storeBlob(BlobDatabase::Entry(entry));
    ASSERT_GT(blobId, static_cast<BlobId>(0));

    // The second storeBlob is expected to fail.
    ASSERT_THROW(
        databaseFactory()->storeBlob(BlobDatabase::Entry(entry)),
        ErpException);
}


TEST_P(BlobDatabaseTest, deleteBlob_failForMissingBlob)
{
    const auto& databaseFactory = std::get<0>(GetParam());

    ASSERT_THROW(
        databaseFactory()->deleteBlob(BlobType::Quote, ErpVector()),
        ErpException);
}


TEST_P(BlobDatabaseTest, getAllBlobsSortedById)
{
    const auto& databaseFactory = std::get<0>(GetParam());

    // Prepare a blob.
    BlobDatabase::Entry entry;
    entry.type = BlobType::EciesKeypair;
    entry.blob.data = SafeString("this is the blob data");
    entry.blob.generation = 1;
    entry.id = 0;

    auto database = databaseFactory();
    size_t initialBlobCount = databaseFactory()->getAllBlobsSortedById().size();

    // Store it n times.
    const size_t n = 10;
    for (uint32_t index=0; index<10; ++index)
    {
        entry.blob.generation = index + 11;
        entry.name = ErpVector::create("name is usually binary data" + std::to_string(index));
        rememberToDeleteBlob(entry.type, entry.name);
        auto const blobId = database->storeBlob(BlobDatabase::Entry(entry)); // Pass in a copy so that we can use entry later on.
        ASSERT_GT(blobId, static_cast<BlobId>(0));
    }

    // Verify that we get all entries back.
    const std::vector<BlobDatabase::Entry> entries = databaseFactory()->getAllBlobsSortedById();
    ASSERT_EQ(entries.size(), n + initialBlobCount);

    // and that they are sorted on their id.
    size_t lastId = 0;
    for (const auto& entry_ : entries)
    {
        ASSERT_GT(entry_.id, lastId);
        lastId = entry_.id;
    }
}


TEST_P(BlobDatabaseTest, hasValidBlobOfTypes)
{
    const auto& databaseFactory = std::get<0>(GetParam());

    // Prepare a blob.
    BlobDatabase::Entry entry;
    entry.type = BlobType::EciesKeypair,
    entry.name = ErpVector::create("name is usually binary data");
    entry.blob.data = SafeString("this is the blob data");
    entry.blob.generation = 1;
    entry.id = 0;

    // Store the blob with the same type but with different generations two times.
    {
        rememberToDeleteBlob(entry.type, entry.name);
        auto const blobId = databaseFactory()->storeBlob(BlobDatabase::Entry(entry));
        ASSERT_GT(blobId, static_cast<BlobId>(0));
    }

    {
        entry.name = ErpVector::create("a different name");
        entry.blob.generation = 2;
        rememberToDeleteBlob(entry.type, entry.name);
        auto const blobId = databaseFactory()->storeBlob(std::move(entry));
        ASSERT_GT(blobId, static_cast<BlobId>(0));
    }

    // Call hasValidBlobsOfTypes with a duplicate and a type that is not in the database.
    const std::vector<bool> flags = databaseFactory()->hasValidBlobsOfType(
        std::vector{BlobType::EciesKeypair, BlobType::VauSig, BlobType::EciesKeypair});

    // Check the result.
    ASSERT_TRUE(flags[0]);
    ASSERT_FALSE(flags[1]);
    ASSERT_TRUE(flags[2]);
}


TEST_P(BlobDatabaseTest, hasValidBlobOfTypes_withExpiry)
{
    const auto& databaseFactory = std::get<0>(GetParam());

    // Prepare a blob.
    BlobDatabase::Entry entry;
    entry.type = BlobType::EciesKeypair,
    entry.name = ErpVector::create("name is usually binary data");
    entry.blob.data = SafeString("this is the blob data");
    entry.blob.generation = 1;
    entry.id = 0;
    entry.expiryDateTime = std::chrono::system_clock::now() + std::chrono::minutes(1);

    auto const blobId = databaseFactory()->storeBlob(BlobDatabase::Entry(entry));
    rememberToDeleteBlob(entry.type, entry.name);
    ASSERT_GT(blobId, static_cast<BlobId>(0));

    // Call hasValidBlobsOfTypes with a duplicate and a type that is not in the database.
    const std::vector<bool> flags = databaseFactory()->hasValidBlobsOfType(
        std::vector{BlobType::EciesKeypair, BlobType::VauSig, BlobType::EciesKeypair});

    // Check the result.
    ASSERT_TRUE(flags[0]);
    ASSERT_FALSE(flags[1]);
    ASSERT_TRUE(flags[2]);
}


TEST_P(BlobDatabaseTest, blob_filterByRelease) // NOLINT(readability-function-cognitive-complexity)
{
    // This test can only work with a real database.
    if (std::get<1>(GetParam()))
        GTEST_SKIP();

    const auto& databaseFactory = std::get<0>(GetParam());

    // Verify that initially there is no EciesKeyPair|1 blob.
    ASSERT_ANY_THROW(
        databaseFactory()->getBlob(BlobType::Quote, 1));

    // Create the blob.
    BlobDatabase::Entry entry;
    entry.type = BlobType::Quote;
    entry.name = ErpVector::create("name is usually binary data");
    entry.blob.data = SafeString("this is the blob data");
    entry.blob.generation = 1;
    entry.expiryDateTime = nowPlus(std::chrono::minutes(1));
    entry.startDateTime = nowPlus(std::chrono::minutes(-1));
    entry.endDateTime = nowPlus(std::chrono::minutes(2));
    entry.id = 0;

    rememberToDeleteBlob(entry.type, entry.name);
    auto const blobId = databaseFactory()->storeBlob(BlobDatabase::Entry(entry)); // Pass in a copy so that we can use entry later on.
    ASSERT_GT(blobId, static_cast<BlobId>(0));

    {
        // Store the (almost) same blob a second time but this time for another release.
        auto copy = entry;
        copy.name = ErpVector::create("another name");
        rememberToDeleteBlob(copy.type, copy.name);
        auto database = ProductionBlobDatabase();
        auto const blobId2 = database.storeBlob(BlobDatabase::Entry(copy)); // Pass in a copy so that we can use entry later on.
        ASSERT_GT(blobId2, static_cast<BlobId>(0));

        auto transaction = database.createTransaction();
        transaction->exec("UPDATE erp.blob SET build = 'other release' WHERE blob_id=" + std::to_string(blobId2));
        transaction->commit();

        // Try to access this second blob via its id and verify that that fails (filtered by release).
        ASSERT_ANY_THROW(
            databaseFactory()->getBlob(BlobType::Quote, blobId2));
    }

    // Get all blobs ...
    const auto entries = databaseFactory()->getAllBlobsSortedById();
    // ... and expect the second quote blob to have been filtered out.
    ASSERT_GE(entries.size(), 1);
    ASSERT_EQ(entries.back().id, blobId);

    // Verify that the first blob is recognized as a valid quote blob.
    const auto flags = databaseFactory()->hasValidBlobsOfType({BlobType::Quote});
    ASSERT_EQ(flags.size(), 1);
    ASSERT_TRUE(flags.front());
}


/**
 * This test focuses on storing and retrieving metadata.
 */
TEST_P(BlobDatabaseTest, blob_crud_withMetadata)//NOLINT(readability-function-cognitive-complexity)
{
    const auto& databaseFactory = std::get<0>(GetParam());

    // Create the blob.
    BlobDatabase::Entry entry;
    entry.type = BlobType::EciesKeypair;
    entry.name = ErpVector::create("name is usually binary data");
    entry.blob.data = SafeString("this is the blob data");
    entry.blob.generation = 1;
    entry.id = 0;
    entry.metaAkName = ErpArray<TpmObjectNameLength>::create("< 012345678901234567890123456789 >");
    entry.pcrSet = PcrSet::fromString("2,4,6");

    // Store it.
    rememberToDeleteBlob(entry.type, entry.name);
    auto const blobId = databaseFactory()->storeBlob(BlobDatabase::Entry(entry)); // Pass in a copy so that we can use entry later on.
    ASSERT_GT(blobId, static_cast<BlobId>(0));

    // Read it.
    const auto retrieved = databaseFactory()->getBlob(entry.type, blobId);
    ASSERT_EQ(retrieved.id, blobId);
    ASSERT_TRUE(retrieved.metaAkName.has_value());
    ASSERT_EQ(retrieved.metaAkName, ErpArray<TpmObjectNameLength>::create("< 012345678901234567890123456789 >"));
    ASSERT_TRUE(retrieved.pcrSet.has_value());
    ASSERT_EQ(retrieved.pcrSet, PcrSet::fromString("2,4,6"));

    // Delete it.
    databaseFactory()->deleteBlob(entry.type, entry.name);
    markAsDeleted(entry.type, entry.name);
}


// There are two instantiations to have prettier names for tests that clearly distinguish between mock and production.
INSTANTIATE_TEST_SUITE_P(
    BlobDatabaseTest_Mock,
    BlobDatabaseTest,
    testing::ValuesIn(getMockDatabaseFactories()));

INSTANTIATE_TEST_SUITE_P(
    BlobDatabaseTest_Production,
    BlobDatabaseTest,
    testing::ValuesIn(getProductionDatabaseFactories()));
