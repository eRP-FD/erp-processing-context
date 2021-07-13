#include "mock/hsm/MockBlobDatabase.hxx"

#include <vector>

#include "erp/util/Expect.hxx"


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

    ErpExpect(mEntries.find(entry.name) == mEntries.end(), HttpStatus::BadRequest, "blob with the same name already exists");

    const auto id = getNextBlobId(entry.type);
    const auto name = entry.name;
    const auto result = mEntries.emplace(name, std::move(entry));
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


BlobId MockBlobDatabase::getNextBlobId (BlobType type) const
{
    // Assume that mMutex is still locked.

    BlobId nextId = 1;
    for (const auto& entry : mEntries)
        if (entry.second.type == type)
            if (entry.second.id >= nextId)
                nextId = entry.second.id + 1;
    return nextId;
}
