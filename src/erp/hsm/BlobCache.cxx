/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/BlobCache.hxx"

#include "erp/util/Hash.hxx"

#include <vector>


namespace
{
    template<class KeyType>
    std::string keyToString (const KeyType key)
    {
        if constexpr (std::is_same_v<KeyType, BlobType>)
            return "'" + std::string(magic_enum::enum_name(key)) + "'";
        else if constexpr (std::is_same_v<KeyType, CacheKey>)
            return "(" + std::string(magic_enum::enum_name(key.type)) + ", " + std::to_string(key.id) + ")";
        else
            return "<unknown>";
    }

    /**
     * Remove one level of std::reference_wrapper from the given value when it is there. Return value
     * unmodified otherwise.
     */
    template<class ValueType>
    BlobCache::Entry& unwrap (ValueType& value)
    {
        if constexpr (std::is_same_v<ValueType, BlobCache::Entry>)
            return value;
        else if constexpr (std::is_same_v<ValueType, std::reference_wrapper<BlobCache::Entry>>)
            return value.get();
        // else fail to compile
    }
}


template<class KeyType, class ValueType>
BlobCache::Entry BlobCache::getBlob (const KeyType key, std::unordered_map<KeyType, ValueType>& map)
{
    // First attempt.
    {
        // Read-only operation.
        std::shared_lock lock (mMutex);

        const auto iterator = map.find(key);
        if (iterator != map.end())
        {
            ErpExpect(unwrap(iterator->second).isBlobValid(), HttpStatus::InternalServerError, "blob " + keyToString(key) + " is not valid");
            return unwrap(iterator->second);
        }
    }

    rebuildCache();

    // Second and last attempt.
    // This is expected to be necessary very infrequently.
    {
        // Read-only operation.
        std::shared_lock lock (mMutex);

        const auto iterator = map.find(key);
        if (iterator != map.end())
        {
            ErpExpect(unwrap<ValueType>(iterator->second).isBlobValid(), HttpStatus::InternalServerError, "blob " + keyToString(key) + " is not valid");
            return unwrap<ValueType>(iterator->second);
        }
    }

    ErpFail(HttpStatus::InternalServerError, "did not find blob " + keyToString(key) + " in blob database");
}


BlobCache::BlobCache (std::unique_ptr<BlobDatabase>&& db)
    : mDatabase(std::move(db)),
      mMutex(),
      mEntries(),
      mNewestEntries()
{
}


BlobCache::Entry BlobCache::getBlob (
    const BlobType type,
    const BlobId id)
{
    return getBlob(CacheKey(type, id), mEntries);
}


BlobCache::Entry BlobCache::getBlob (
    const BlobType type)
{
    return getBlob(type, mNewestEntries);
}


BlobCache::Entry BlobCache::getAttestationKeyPairBlob (
    const std::function<ErpBlob()>& blobProvider,
    const bool isEnrolmentActive)
{
    // Quick and early return in the likely case that the attestation key pair is already in the cache.
    {
        // Read-only operation.
        std::shared_lock lock (mMutex);

        const auto iterator = mNewestEntries.find(BlobType::AttestationKeyPair);
        if (iterator != mNewestEntries.end())
            return iterator->second;
    }

    if ( ! isEnrolmentActive)
    {
        // The attestation key pair can only be created during the enrolment process.
        // That is currently not active, so throw an exception.
        // BadRequest because during the enrolment process the processing context should not be open to the public
        // and calls to its VAU interface should not come in.
        ErpFail(HttpStatus::BadRequest, "attestation key pair is missing and can not be created");
    }

    // From here on we can assume that an enrolment is ongoing. That means that calls come in only from a single thread
    // in a single process (which services the enrolment api). Locking of the database is not necessary as collisions
    // between multiple calls that all try to create a new attestation token should not be possible.

    // Create a new attestation key pair blob.
    TLOG(WARNING) << "creating a new attestation token";
    Entry entry;
    entry.type = BlobType::AttestationKeyPair;
    entry.blob = blobProvider();
    const auto hash = Hash::sha256(entry.blob.data);
    entry.name = ErpVector(hash.begin(), hash.end());

    // Store the new blob.
    {
        std::unique_lock lock(mDatabaseMutex);
        entry.id = mDatabase->storeBlob(std::move(entry));
    }

    // Rebuild the cache and make another lookup of the attestation key pair blob.
    rebuildCache();
    {
        // Read-only operation.
        std::shared_lock lock (mMutex);

        const auto iterator = mNewestEntries.find(BlobType::AttestationKeyPair);
        Expect(iterator != mNewestEntries.end(), "did not create attestation key pair blob");
        return iterator->second;
    }
}



BlobId BlobCache::storeBlob (BlobDatabase::Entry&& entry)
{
    std::unique_lock lock(mDatabaseMutex);
    const auto newId = mDatabase->storeBlob(std::move(entry));
    lock.unlock();

    rebuildCache();
    return newId;
}


void BlobCache::deleteBlob (
    const BlobType type,
    const ErpVector& name)
{
    {
        std::unique_lock lock(mDatabaseMutex);
        mDatabase->deleteBlob(type, name);
    }
    rebuildCache();
}


std::vector<bool> BlobCache::hasValidBlobsOfType (std::vector<BlobType>&& blobTypes) const
{
    std::shared_lock lock(mDatabaseMutex);
    return mDatabase->hasValidBlobsOfType(std::move(blobTypes));
}


void BlobCache::rebuildCache (void)
{
    // First, without locking the mutex, ask the database for all its blobs and rebuild the two maps from it.
    // As the database can be manipulated by other instances of the processing context on the same and on other machines,
    // we can't synchronize cache and db perfectly. Therefore don't even try and instead minimize the time we lock the mutex.
    std::unordered_map<CacheKey, Entry> entries;
    std::unordered_map<BlobType, std::reference_wrapper<Entry>> newestEntries;

    std::shared_lock dbLock(mDatabaseMutex);
    auto allBlobsSortedById = mDatabase->getAllBlobsSortedById();
    dbLock.unlock();

    for (auto&& entry : allBlobsSortedById)
    {
        const auto result = entries.emplace(CacheKey(entry.type, entry.id), std::move(entry));
        ErpExpect(result.second, HttpStatus::InternalServerError, "rebuilding of blob cache failed");
        newestEntries.insert_or_assign(result.first->second.type, result.first->second);
    }

    // Now swap the maps.
    {
        std::unique_lock lock (mMutex);
        mEntries.swap(entries);
        mNewestEntries.swap(newestEntries);
    }
}


CacheKey::CacheKey (const BlobType t, const BlobId i)
    :type(t),
     id(i)
{
}


bool CacheKey::operator== (const CacheKey& other) const
{
    return type==other.type
           && id==other.id;
}


size_t CacheKey::hash (void) const
{
    return std::hash<BlobType>()(type)
         ^ std::hash<BlobId>()(id);
}
