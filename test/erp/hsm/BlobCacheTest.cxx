/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#include <gtest/gtest.h>

#include "shared/hsm/BlobCache.hxx"

#include "shared/util/Expect.hxx"

#include <atomic>
#include <test/util/TestUtils.hxx>

using namespace ::std::chrono_literals;

namespace {
    std::atomic_int getBlobCallcount = 0;
    std::atomic_int getAllBlobsCallcount = 0;
}


class TestBlobDatabase : public BlobDatabase
{
public:
    std::vector<Entry> mEntries;

    Entry getBlob (
        BlobType type,
        BlobId id) const override
    {
        getBlobCallcount++;

        for (const auto& entry : mEntries)
            if (entry.type==type && entry.id==id)
                return entry;
        Fail("unknwon blob");
    }

    Entry getBlob(BlobType type, const ErpVector& name) const override
    {
        for (const auto& entry : mEntries)
            if (entry.type == type && entry.name == name)
                return entry;
        Fail("unknwon blob");
    }

    std::vector<Entry> getAllBlobsSortedById (void) const override
    {
        getAllBlobsCallcount++;
        auto entries = mEntries;

        std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b)
            {
              return a.id <= b.id;
            });

        return entries;
    }

    BlobId storeBlob (Entry&& newEntry) override
    {
        for (const auto& entry : mEntries)
            Expect(entry.name != newEntry.name, "blob with the same name already in the database");
        const auto id = getNextBlobId(newEntry.type);
        mEntries.emplace_back(newEntry);
        mEntries.back().id = id;
        return id;
    }

    void deleteBlob (const BlobType type, const ErpVector& name) override
    {
        for (auto iterator=mEntries.begin(); iterator!=mEntries.end(); ++iterator)
            if (iterator->name == name)
            {
                Expect(iterator->type == type, "blob matches name but not type");
                mEntries.erase(iterator);
                return;
            }
        Fail("blob not found");
    }

    std::vector<bool> hasValidBlobsOfType (std::vector<BlobType>&&) const override
    {
        return {};
    }

    BlobId getNextBlobId (BlobType type) const
    {
        BlobId nextId = 1;
        for (const auto& entry : mEntries)
            if (entry.type == type)
                if (entry.id >= nextId)
                    nextId = entry.id + 1;
        return nextId;
    }

    void recreateConnection() override
    {
    }
};


class BlobCacheTest : public testing::Test
{
public:
    void SetUp (void) override
    {
        auto db = std::make_unique<TestBlobDatabase>();
        database = db.get();
        cache = std::make_unique<BlobCache>(std::move(db));
        getBlobCallcount = 0;
        getAllBlobsCallcount = 0;
    }

    BlobDatabase::Entry createTestBlob (
        const std::string& blobKey = "blob-key",
        const BlobId blobId = 2)
    {
        return BlobDatabase::Entry({
            BlobType::TaskKeyDerivation,
            ErpVector::create(blobKey),
            ErpBlob{"blob-data", 3},
            {},{},{},
            blobId,
            {}, {}, {}, {}});
    }

    TestBlobDatabase* database = nullptr;
    std::unique_ptr<BlobCache> cache;
};



TEST_F(BlobCacheTest, getBlob_success) // NOLINT(readability-function-cognitive-complexity)
{
    database->mEntries.emplace_back(createTestBlob());

    ASSERT_EQ(getBlobCallcount, 0);

    // Ask for a blob that is in the database...
    const auto entry = cache->getBlob(BlobType::TaskKeyDerivation, 2);
    ASSERT_EQ(entry.id, static_cast<BlobId>(2));
    ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(3));
    // ... and verify that the database has been called by the cache to retrieve it.
    ASSERT_EQ(getBlobCallcount, 0);
    ASSERT_EQ(getAllBlobsCallcount, 1);

    // Ask for the same blob a second time and verify that the database has not been called again.
    cache->getBlob(BlobType::TaskKeyDerivation, 2);
    ASSERT_EQ(getBlobCallcount, 0);
    ASSERT_EQ(getAllBlobsCallcount, 1);

    ASSERT_NO_THROW(cache->getBlob(2));
    EXPECT_EQ(getBlobCallcount, 0);
    EXPECT_EQ(getAllBlobsCallcount, 1);
}

TEST_F(BlobCacheTest, getBlob_allowUsedInvalidBlobs)// NOLINT(readability-function-cognitive-complexity)
{
    database->mEntries.emplace_back(BlobDatabase::Entry({::BlobType::TaskKeyDerivation,
                                                         ::ErpVector::create("blob-key"),
                                                         ::ErpBlob{"blob-data", 3},
                                                         ::std::chrono::system_clock::now() - 5min,
                                                         {},
                                                         {},
                                                         3,
                                                         {},
                                                         {},
                                                         {},
                                                         {}}));

    ASSERT_NO_THROW(cache->getBlob(BlobType::TaskKeyDerivation, 3));
    ASSERT_NO_THROW(cache->getBlob(3));
}

TEST_F(BlobCacheTest, getBlob_ignoreNewerInvalidBlobs)// NOLINT(readability-function-cognitive-complexity)
{
    database->mEntries.emplace_back(createTestBlob());
    database->mEntries.emplace_back(BlobDatabase::Entry({::BlobType::TaskKeyDerivation,
                                                         ::ErpVector::create("blob-key"),
                                                         ::ErpBlob{"blob-data", 3},
                                                         {},
                                                         ::std::chrono::system_clock::now() + 5min,
                                                         {},
                                                         3,
                                                         {},
                                                         {},
                                                         {},
                                                         {}}));

    ASSERT_NO_THROW(cache->getBlob(3));

    EXPECT_NO_THROW(cache->getBlob(::BlobType::TaskKeyDerivation));
    EXPECT_EQ(cache->getBlob(::BlobType::TaskKeyDerivation).id, 2);
}

TEST_F(BlobCacheTest, getBlob_failForMissingBlob)// NOLINT(readability-function-cognitive-complexity)
{
    // Ask for a blob that is NOT in the database.
    ASSERT_ANY_THROW(cache->getBlob(::BlobType::CommunicationKeyDerivation));
    ASSERT_ANY_THROW(cache->getBlob(::BlobType::CommunicationKeyDerivation, 2));
    ASSERT_ANY_THROW(cache->getBlob(::BlobType::TaskKeyDerivation, 12));
    ASSERT_ANY_THROW(cache->getBlob(12));
}


TEST_F(BlobCacheTest, getBlob_failForInvalidBlob)// NOLINT(readability-function-cognitive-complexity)
{
    database->mEntries.emplace_back(
        BlobDatabase::Entry({BlobType::TaskKeyDerivation,
                             ErpVector::create("blob-key"),
                             ErpBlob{"blob-data", 3},
                             std::chrono::system_clock::now() - std::chrono::seconds(1),// blob expired a second ago
                             {},
                             {},
                             2,
                             {},
                             {},
                             {},
                             {}}));

    EXPECT_NO_THROW(cache->getBlob(BlobType::TaskKeyDerivation, 2));
    EXPECT_NO_THROW(cache->getBlob(2));
    EXPECT_ANY_THROW(cache->getBlob(BlobType::TaskKeyDerivation));
}

TEST_F(BlobCacheTest, getEciesKeysSingle)// NOLINT(readability-function-cognitive-complexity)
{
    ASSERT_ANY_THROW(cache->getEciesKeys());

    database->mEntries.emplace_back(BlobDatabase::Entry({::BlobType::EciesKeypair,
                                                         ::ErpVector::create("blob-key"),
                                                         ::ErpBlob{"blob-data", 3},
                                                         {},
                                                         {},
                                                         {},
                                                         1,
                                                         {},
                                                         {},
                                                         {},
                                                         {}}));

    ASSERT_NO_THROW(cache->getEciesKeys());
    const auto eciesKeys = cache->getEciesKeys();
    ASSERT_EQ(eciesKeys.latest.id, 1);
    ASSERT_FALSE(eciesKeys.fallback);
}

TEST_F(BlobCacheTest, getEciesKeysMultiple)// NOLINT(readability-function-cognitive-complexity)
{
    ASSERT_ANY_THROW(cache->getEciesKeys());

    database->mEntries.emplace_back(BlobDatabase::Entry({::BlobType::EciesKeypair,
                                                         ::ErpVector::create("blob-key-1"),
                                                         ::ErpBlob{"blob-data", 3},
                                                         {},
                                                         {},
                                                         {},
                                                         1,
                                                         {},
                                                         {},
                                                         {},
                                                         {}}));

    database->mEntries.emplace_back(BlobDatabase::Entry({::BlobType::EciesKeypair,
                                                         ::ErpVector::create("blob-key-2"),
                                                         ::ErpBlob{"blob-data", 3},
                                                         {},
                                                         {},
                                                         {},
                                                         2,
                                                         {},
                                                         {},
                                                         {},
                                                         {}}));

    database->mEntries.emplace_back(BlobDatabase::Entry({::BlobType::EciesKeypair,
                                                         ::ErpVector::create("blob-key-3"),
                                                         ::ErpBlob{"blob-data", 3},
                                                         {},
                                                         {},
                                                         {},
                                                         3,
                                                         {},
                                                         {},
                                                         {},
                                                         {}}));

    ASSERT_NO_THROW(cache->getEciesKeys());
    const auto eciesKeys = cache->getEciesKeys();
    ASSERT_EQ(eciesKeys.latest.id, 3);
    ASSERT_TRUE(eciesKeys.fallback);
    ASSERT_EQ(eciesKeys.fallback.value().id, 2);
}

TEST_F(BlobCacheTest, storeBlob_success)
{
    BlobDatabase::Entry entry;
    entry.type = BlobType::TaskKeyDerivation;
    entry.name = ErpVector::create("blob-key");
    entry.blob = ErpBlob{"blob-data", 3};
    const auto id = cache->storeBlob(std::move(entry));

    // Verify that the new entry was stored in the cache, i.e. no call to the database's getBlob is made, ...
    cache->getBlob(BlobType::TaskKeyDerivation, id);
    ASSERT_EQ(getBlobCallcount, 0);
    // ... and also in the database.
    ASSERT_FALSE(database->mEntries.empty());
}


TEST_F(BlobCacheTest, storeBlob_failForSecondStore)
{
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::TaskKeyDerivation;
        entry.name = ErpVector::create("blob-key");
        entry.blob = ErpBlob{"blob-data", 3};
        const auto id = cache->storeBlob(std::move(entry));
        ASSERT_NE(id, static_cast<BlobId>(0));
    }

    // Store a blob under the same name a second time. Note that both generation and id are different.
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::TaskKeyDerivation;
        entry.name = ErpVector::create("blob-key");
        entry.blob = ErpBlob{"blob-data", 4};
        ASSERT_ANY_THROW(cache->storeBlob(std::move(entry)));
    }
}


TEST_F(BlobCacheTest, deleteBlob_success)// NOLINT(readability-function-cognitive-complexity)
{
    // Add a blob to the database that is to be deleted, then pull it into the cache.
    database->mEntries.emplace_back(createTestBlob("blob-key", 2));
    database->mEntries.emplace_back(createTestBlob("other-blob-key", 3));
    cache->getBlob(BlobType::TaskKeyDerivation, 2);
    ASSERT_EQ(database->mEntries.size(), 2);

    // Delete the blob from the cache. As the cache is write-through that should delete the entry also from the database.
    cache->deleteBlob(BlobType::TaskKeyDerivation, ErpVector::create("blob-key"));

    // Verify that it is not in the database anymore ...
    ASSERT_EQ(database->mEntries.size(), 1);
    // ... and that it is not in the cache anymore either.
    ASSERT_ANY_THROW(cache->getBlob(BlobType::TaskKeyDerivation, 2));
    ASSERT_ANY_THROW(cache->getBlob(2));
}


TEST_F(BlobCacheTest, deleteBlob_failForMissingBlob)
{
    // Create a blob that will NOT be deleted.
    database->mEntries.emplace_back(createTestBlob());
    const auto entry = cache->getBlob(BlobType::TaskKeyDerivation, 2);
    ASSERT_NE(entry.id, static_cast<BlobId>(0));

    ASSERT_ANY_THROW(cache->deleteBlob(BlobType::TaskKeyDerivation, ErpVector::create("other-blob-key")));
}

TEST_F(BlobCacheTest, updateAfterKeyExpired)// NOLINT(readability-function-cognitive-complexity)
{
    using namespace std::chrono;
    auto expiresAt = system_clock::now() + 200ms;
    database->mEntries.emplace_back(BlobDatabase::Entry({BlobType::TaskKeyDerivation,
                                                         ErpVector::create("blob-key"),
                                                         ErpBlob{"blob-data", 3},
                                                         expiresAt,
                                                         {},
                                                         {},
                                                         2,
                                                         {},
                                                         {},
                                                         {},
                                                         {}}));
    ASSERT_NO_THROW(cache->getBlob(BlobType::TaskKeyDerivation));
    testutils::waitFor([&] {
        return system_clock::now() > expiresAt;
    });
    ASSERT_ANY_THROW(cache->getBlob(BlobType::TaskKeyDerivation));
    expiresAt = system_clock::now() + 5s;
    database->mEntries.emplace_back(BlobDatabase::Entry({BlobType::TaskKeyDerivation,
                                                         ErpVector::create("blob-key"),
                                                         ErpBlob{"blob-data", 3},
                                                         expiresAt,
                                                         {},
                                                         {},
                                                         3,
                                                         {},
                                                         {},
                                                         {},
                                                         {}}));
    ASSERT_NO_THROW(cache->getBlob(BlobType::TaskKeyDerivation));
}


TEST_F(BlobCacheTest, updateBeforeKeyExpired)// NOLINT(readability-function-cognitive-complexity)
{
    using namespace std::chrono;
    auto expiresAt = system_clock::now() + 5s;
    database->mEntries.emplace_back(BlobDatabase::Entry({BlobType::TaskKeyDerivation,
                                                         ErpVector::create("blob-key"),
                                                         ErpBlob{"blob-data", 3},
                                                         expiresAt,
                                                         {},
                                                         {},
                                                         2,
                                                         {},
                                                         {},
                                                         {},
                                                         {}}));
    ASSERT_NO_THROW(cache->getBlob(BlobType::TaskKeyDerivation));
    expiresAt = system_clock::now() + 5s;
    database->mEntries.emplace_back(BlobDatabase::Entry({BlobType::TaskKeyDerivation,
                                                         ErpVector::create("blob-key"),
                                                         ErpBlob{"blob-data", 3},
                                                         expiresAt,
                                                         {},
                                                         {},
                                                         3,
                                                         {},
                                                         {},
                                                         {},
                                                         {}}));
    boost::asio::io_context ioContext;
    cache->startRefresher(ioContext, 500ms);
    std::thread contextThread{[&] {
        ioContext.run();
    }};
    EXPECT_NO_FATAL_FAILURE(testutils::waitFor([&] {
                                auto entry = cache->getBlob(BlobType::TaskKeyDerivation);
                                return entry.id == 3;
                            }););
    ioContext.stop();
    contextThread.join();
}

TEST_F(BlobCacheTest, filterPcrHash)// NOLINT(readability-function-cognitive-complexity)
{
    const auto name = ::ErpVector::create("blob-key");
    cache->storeBlob(
        ::BlobDatabase::Entry{::BlobType::Quote, name, ::ErpBlob{"blob-data", 3}, {}, {}, {}, 2, {}, {}, {}, {}});

    EXPECT_NO_THROW(cache->getBlob(::BlobType::Quote));

    cache->deleteBlob(::BlobType::Quote, name);
    EXPECT_ANY_THROW(cache->getBlob(::BlobType::Quote));

    const auto pcrHash = ::ErpVector::create("123456");
    cache->storeBlob(
        ::BlobDatabase::Entry{::BlobType::Quote, name, ::ErpBlob{"blob-data", 3}, {}, {}, {}, 2, {}, {}, pcrHash, {}});

    EXPECT_NO_THROW(cache->getBlob(::BlobType::Quote));

    cache->setPcrHash(pcrHash);

    EXPECT_NO_THROW(cache->getBlob(::BlobType::Quote));

    cache->setPcrHash(::ErpVector::create("abcdef"));

    EXPECT_ANY_THROW(cache->getBlob(::BlobType::Quote));
}

// There is not much sense in testing hasValidBlobsOfType() as that is just forwarding to the database without any
// processing of input or output values.
