/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/BlobCache.hxx"

#include "erp/util/Hash.hxx"
#include "erp/util/PeriodicTimer.hxx"

#include <boost/range/adaptor/reversed.hpp>
#include <algorithm>
#include <utility>
#include <vector>


namespace
{
template<typename Key>
::std::string toString(Key key);

template<>
::std::string toString<::BlobType>(::BlobType key)
{
    return ::std::string{" of type "} + ::std::string{::magic_enum::enum_name(key)};
}

template<>
::std::string toString<::BlobId>(::BlobId key)
{
    return ::std::string{" with id "} + ::std::to_string(key);
}

bool defaultValidityChecker(const ::BlobCache::Entry& entry)
{
    return entry.isBlobValid();
}
}

class BlobCache::Refresher : public FixedIntervalHandler
{
public:
    explicit Refresher(BlobCache& blobCache, std::chrono::steady_clock::duration interval)
        : FixedIntervalHandler{interval}
        , mBlobCache{blobCache}
    {
    }

private:
    void timerHandler() override
    {
        TLOG(INFO) << "Refreshing Blob-Cache.";
        try
        {
            mBlobCache.rebuildCache();
        }
        catch (const std::runtime_error& error)
        {
            TLOG(ERROR) << "failed to refresh the blob cache: " << error.what();
        }
    }
    BlobCache& mBlobCache;
};

BlobCache::BlobCache(std::unique_ptr<BlobDatabase>&& db)
    : mDatabase(std::move(db))
{
}

BlobCache::~BlobCache() = default;

BlobCache::Entry BlobCache::getBlob(const BlobType type)
{
    // blob for new entry, do not allow invalid blobs
    return getBlobs(mEntriesByType, type, defaultValidityChecker).front();
}

BlobCache::Entry BlobCache::getBlob(const BlobType type, const BlobId id)
{
    return getBlobs(mEntriesById, id,
                    [type](const ::BlobCache::Entry& entry) {
                        // blob was already used, also allow blobs going invalid in the meantime
                        return (entry.type == type);
                    })
        .front();
}

BlobCache::Entry BlobCache::getBlob(BlobId id)
{
    // blob was already used, also allow blobs going invalid in the meantime
    return getBlobs(mEntriesById, id).front();
}

BlobCache::EciesKeys BlobCache::getEciesKeys()
{
    BlobCache::EciesKeys keys;

    auto blobs = getBlobs(mEntriesByType, ::BlobType::EciesKeypair, defaultValidityChecker, 2);

    keys.latest = blobs.front();
    if (blobs.size() > 1)
    {
        keys.fallback = blobs[1];
    }

    return keys;
}

BlobId BlobCache::storeBlob(BlobDatabase::Entry&& entry)
{
    const auto newId = [this](::BlobDatabase::Entry&& entry) {
        ::std::unique_lock lock{mDatabaseMutex};
        return mDatabase->storeBlob(::std::move(entry));
    }(::std::move(entry));

    rebuildCache();
    return newId;
}

void BlobCache::deleteBlob(const BlobType type, const ErpVector& name)
{
    {
        std::unique_lock lock(mDatabaseMutex);
        mDatabase->deleteBlob(type, name);
    }

    rebuildCache();
}

std::vector<BlobDatabase::Entry> BlobCache::getAllBlobsSortedById (void) const
{
    ::std::unique_lock lock(mDatabaseMutex);
    return mDatabase->getAllBlobsSortedById();
}

void BlobCache::setPcrHash(const ::ErpVector& pcrHash)
{
    if (mPcrHash != pcrHash)
    {
        ::std::shared_lock lock(mMutex);
        mPcrHash = pcrHash;
        lock.unlock();

        rebuildCache();
    }
}

void BlobCache::startRefresher(boost::asio::io_context& context, std::chrono::steady_clock::duration interval)
{
    Expect3(mRefresher == nullptr, "refresher already started", std::logic_error);
    mRefresher = std::make_unique<PeriodicTimer<Refresher>>(*this, interval);
    mRefresher->start(context, interval);
}

std::vector<bool> BlobCache::hasValidBlobsOfType(std::vector<BlobType>&& blobTypes) const
{
    std::unique_lock lock(mDatabaseMutex);
    return mDatabase->hasValidBlobsOfType(std::move(blobTypes));
}


void BlobCache::rebuildCache(void)
{
    // First, without locking the mutex, ask the database for all its blobs and rebuild the two maps from it.
    // As the database can be manipulated by other instances of the processing context on the same and on other machines,
    // we can't synchronize cache and db perfectly. Therefore don't even try and instead minimize the time we lock the mutex.
    decltype(mEntriesById) entriesById;
    decltype(mEntriesByType) entriesByType;

    const auto allBlobsSortedById = [this]() {
        ::std::unique_lock dbLock{mDatabaseMutex};
        return mDatabase->getAllBlobsSortedById();
    }();

    for (auto&& entry : ::boost::adaptors::reverse(allBlobsSortedById))
    {
        const auto [item, inserted] = entriesById.emplace(entry.id, entry);
        ErpExpect(inserted, ::HttpStatus::InternalServerError, "rebuilding of blob cache failed");

        // Search by type is only done for new database entries where we do not use invalid blobs.
        if (item->second.isBlobValid(::std::chrono::system_clock::now(), mPcrHash))
        {
            entriesByType.emplace(item->second.type, item->second);
        }
    }

    ::std::unique_lock lock(mMutex);
    mEntriesById.swap(entriesById);
    mEntriesByType.swap(entriesByType);
}

template<class Map, typename Key>
::std::vector<::BlobCache::Entry> BlobCache::findBlobs(const Map& map, Key key, const ValidityChecker& validityChecker,
                                                       ::std::size_t count)
{
    ::std::vector<::BlobCache::Entry> entries;
    entries.reserve(count);

    ::std::shared_lock lock(mMutex);

    const auto [begin, end] = map.equal_range(key);
    for (auto iterator = begin; (iterator != end) && (entries.size() < count); ++iterator)
    {
        if (! validityChecker || validityChecker(iterator->second))
        {
            entries.push_back(iterator->second);
        }
    }

    return entries;
}

template<class Map, typename Key>
::std::vector<::BlobCache::Entry> BlobCache::getBlobs(const Map& map, Key key, const ValidityChecker& validityChecker,
                                                      ::std::size_t count)
{
    auto entries = findBlobs(map, key, validityChecker, count);

    if (entries.empty())
    {
        rebuildCache();

        entries = findBlobs(map, key, validityChecker, count);
        ErpExpect(! entries.empty(), ::HttpStatus::InternalServerError,
                  "did not find blob " + toString(key) + " in blob database");
    }

    return entries;
}
