/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_BLOBCACHE_HXX
#define ERP_PROCESSING_CONTEXT_BLOBCACHE_HXX

#include "erp/hsm/BlobDatabase.hxx"

#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <vector>

class PeriodicTimerBase;

/**
 * This cache is intended as the primary interface to access blobs in the blob database, either for hsm or
 * enrolment operations. It reads from an underlying (and yet to come) database on-demand and writes-through incoming
 * values to it.

 * Optimize lookups to the database under the assumptions that
 * - there are many reads and few writes
 * - existing entries do not get updated
 *
 * Look up of blob cache entries is done via three different kinds of keys.
 * 1. Calls to the enrolment API use the unique `name` value of the blob, basically a SHA256 hash value.
 * 2. hsm operations make look ups with the blob type and an id, that is maintained solely by the BlobDatabase. This id
 *   is somewhat similar to a blob's generation and is increased whenever a new generation of that blob is stored. But it
 *   is not identical to the generation id.
 * 3. some hsm operations make look ups with only the blob type to retrieve the newest blob of its kind.
 *
 * Because of the expected small number of writes the complexity of the cache implementation is kept small by not
 * updating the cache tables every time the database is modified. Instead the whole cache is rebuilt from the current
 * content of the database. For an expecting number of entries of 10 to 20 and updates being made every 3 months, this
 * should not pose a problem.
 */
class BlobCache
{
public:
    using Entry = BlobDatabase::Entry;

    struct EciesKeys {
        Entry latest;
        ::std::optional<Entry> fallback;
    };

    explicit BlobCache(std::unique_ptr<BlobDatabase>&& db);
    ~BlobCache();

    /**
     * Return the latest blob for the given type.
     */
    Entry getBlob(::BlobType type);

    /**
     * Return the requested blob which is identified by type and blob id. Throws an exception if
     * there is no blob for the given type and id
     *
     * @throws if either the requested blob is neither in the cache nor in the database
     */
    Entry getBlob(::BlobType type, ::BlobId id);
    Entry getBlob(BlobType type, const ErpVector& name);

    Entry getBlob(::BlobId id);

    EciesKeys getEciesKeys();

    /**
     * Return whether there are valid blobs for the given blob types.
     * To be valid a blob must
     * a) exist
     * b) if it has an expiry date that must lie in the future
     * c) if it has a start/end time, the interval must include `now`.
     */
    std::vector<bool> hasValidBlobsOfType(std::vector<BlobType>&& blobTypes) const;

    /**
     * Store the given new entry first in the database and rebuild the cache.
     */
    BlobId storeBlob(BlobDatabase::Entry&& entry);

    /**
     * Delete the given new entry first in the database and rebuild the cache.
     */
    void deleteBlob(BlobType type, const ErpVector& name);

    std::vector<Entry> getAllBlobsSortedById (void) const;

    void setPcrHash(const ::ErpVector& pcrHash);

    void startRefresher(boost::asio::io_context& context, std::chrono::steady_clock::duration interval);

    BlobDatabase& getBlobDatabase();

private:
    mutable ::std::shared_mutex mDatabaseMutex{};
    ::std::unique_ptr<BlobDatabase> mDatabase{};
    // shared_mutex to optimize for the much more frequent read-only calls.
    mutable std::shared_mutex mMutex{};
    ::ErpVector mPcrHash{};
    // For the lookup per blob type alone.
    ::std::map<::BlobId, Entry> mEntriesById{};
    // For the lookup per blob type.
    ::std::multimap<::BlobType, ::std::reference_wrapper<Entry>> mEntriesByType{};

    class Refresher;
    std::unique_ptr<PeriodicTimerBase> mRefresher{};

    void rebuildCache(void);

    using ValidityChecker = ::std::function<bool(const ::BlobCache::Entry& entry)>;

    template<class Map, typename Key>
    ::std::vector<::BlobCache::Entry> findBlobs(const Map& map, Key key, const ValidityChecker& validityChecker,
                                                ::std::size_t count);

    template<class Map, typename Key>
    ::std::vector<::BlobCache::Entry> getBlobs(const Map& map, Key key, const ValidityChecker& validityChecker = {},
                                               ::std::size_t count = 1);
};

#endif
