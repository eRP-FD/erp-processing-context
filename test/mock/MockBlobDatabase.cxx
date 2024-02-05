/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/mock/MockBlobDatabase.hxx"

#include <vector>

#include "erp/hsm/production/ProductionBlobDatabase.hxx"
#include "erp/util/Expect.hxx"
#include "test/util/TestConfiguration.hxx"

BlobDatabase::Entry MockBlobDatabase::getBlob (
    BlobType type,
    BlobId id) const
{
    std::lock_guard lock (mMutex);

    for (const auto& entry : mEntries)
    {
        if (entry.second.type == type && entry.second.id == id)
            return entry.second;
    }

    Fail("blob not found");
}

BlobDatabase::Entry MockBlobDatabase::getBlob(BlobType type, const ErpVector& name) const
{
    std::lock_guard lock(mMutex);

    for (const auto& entry : mEntries)
    {
        if (entry.second.type == type && entry.first == name)
            return entry.second;
    }

    Fail("blob not found");
}


std::vector<BlobDatabase::Entry> MockBlobDatabase::getAllBlobsSortedById (void) const
{
    std::vector<Entry> entries;

    {
        std::lock_guard lock (mMutex);

        entries.reserve(mEntries.size());
        for (const auto& entry : mEntries)
            entries.emplace_back(entry.second);
    }

    std::sort(entries.begin(), entries.end(),
        [](const auto& a, const auto& b)
        {
          return a.id < b.id;
        });

    return entries;
}


BlobId MockBlobDatabase::storeBlob (Entry&& entry)
{
    // Note that entry.id is ignored and will be replaced with a DB auto generated id.

    std::lock_guard lock (mMutex);

    ErpExpect(mEntries.find(entry.name) == mEntries.end(), HttpStatus::Conflict, "blob with the same name already exists");

    const auto id = getNextBlobId(lock);
    const auto name = entry.name;
    const auto result = mEntries.emplace(name, entry);
    Expect(result.second, "storing of blob failed");

    result.first->second.id = id;
    return id;
}


void MockBlobDatabase::deleteBlob (
    const BlobType type,
    const ErpVector& name)
{
    std::lock_guard lock (mMutex);
    const auto iterator = mEntries.find(name);

    ErpExpect(iterator != mEntries.end(), HttpStatus::NotFound, "did not find blob that is to be deleted");
    ErpExpect(iterator->second.type == type, HttpStatus::NotFound, "blob has the wrong type");
    if (mIsBlobInUse && mIsBlobInUse(iterator->second.id))
    {
        ErpFail(HttpStatus::Conflict, "Key is still in use");
    }

    mEntries.erase(iterator);
}



std::vector<bool> MockBlobDatabase::hasValidBlobsOfType (std::vector<BlobType>&& blobTypes) const
{
    std::lock_guard lock (mMutex);

    std::vector<bool> result;

    const auto now = std::chrono::system_clock::now();
    for (const auto type : blobTypes)
    {
        bool found = false;
        // The expected small number of blobs together with the expected infrequent use of this method should
        // make this loop tolerable.
        for (const auto& entry : mEntries)
            if (entry.second.type == type)
            {
                if (entry.second.isBlobValid(now))
                {
                    found = true;
                    break;
                }
            }

        result.emplace_back(found);
    }

    return result;
}

void MockBlobDatabase::setBlobInUseTest(std::function<bool (BlobId)> isBlobInUseFunction)
{
    mIsBlobInUse = std::move(isBlobInUseFunction);
}

BlobId MockBlobDatabase::getNextBlobId(std::lock_guard<std::mutex>&)
{
    return ++mNextBlobId;
}

std::shared_ptr< BlobCache > MockBlobDatabase::createBlobCache(const MockBlobCache::MockTarget target)
{
    if (not TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
        return MockBlobCache::createAndSetup(target, std::make_unique<MockBlobDatabase>());
    else
        return MockBlobCache::createAndSetup(target, std::make_unique<ProductionBlobDatabase>());
}

void MockBlobDatabase::recreateConnection()
{
}
