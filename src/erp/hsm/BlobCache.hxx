/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_BLOBCACHE_HXX
#define ERP_PROCESSING_CONTEXT_BLOBCACHE_HXX

#include "erp/hsm/BlobDatabase.hxx"

#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>
#include <shared_mutex>


/**
 * The CacheKey models the lookup of a blob via type and id.
 */
struct CacheKey
{
    BlobType type;
    BlobId id;
    CacheKey (const BlobType type, const BlobId id);
    bool operator== (const CacheKey& other) const;
    size_t hash (void) const;
};

namespace std
{
    template <>
    struct hash<CacheKey>
    {
        std::size_t operator() (const CacheKey& key) const
        {
            return key.hash();
        }
    };
}


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


    explicit BlobCache (std::unique_ptr<BlobDatabase>&& db);
    ~BlobCache();

    /**
     * Return the requested blob which is identified by type and blob id. Throws an exception if
     * - there is no blob for the given type and id
     * - the blob is not valid due to expiry, start or end date time.
     *
     * @throws if either the requested blob is neither in the cache nor in the database or if the blob exists but is
     *  not valid yet or not anymore, due to optional time date metadata.
     */
    Entry getBlob (
        BlobType type,
        BlobId id);

    /**
     * Return the latest blob for the given type.
     */
    Entry getBlob (
        BlobType type);

    Entry getBlob(BlobId id);

    /**
     * Return an entry that contains the attestation key pair blob or throw an exception.
     * The `isEnrolmentActive` flag tells the method whether it is allowed to create a new key pair, if it does not yet exist.
     * That is only permitted to be done during the enrolment procedure.
     */
    Entry getAttestationKeyPairBlob (
        const std::function<ErpBlob()>& blobProvider,
        const bool isEnrolmentActive);

    /**
     * Return whether there are valid blobs for the given blob types.
     * To be valid a blob must
     * a) exist
     * b) if it has an expiry date that must lie in the future
     * c) if it has a start/end time, the interval must include `now`.
     */
    std::vector<bool> hasValidBlobsOfType (std::vector<BlobType>&& blobTypes) const;

    /**
     * Store the given new entry first in the database and rebuild the cache.
     */
    BlobId storeBlob (BlobDatabase::Entry&& entry);

    /**
     * Delete the given new entry first in the database and rebuild the cache.
     */
    void deleteBlob (
        BlobType type,
        const ErpVector& name);

    void setPcrHash(const ::ErpVector& pcrHash);

    void startRefresher(boost::asio::io_context& context, std::chrono::steady_clock::duration interval);

private:
    mutable std::shared_mutex mDatabaseMutex;
    std::unique_ptr<BlobDatabase> mDatabase;
    // shared_mutex to optimize for the much more frequent read-only calls.
    mutable std::shared_mutex mMutex;
    /// For the lookup per blob type and id.
    std::unordered_map<CacheKey, Entry> mEntries;
    /// For the lookup per blob type alone (references the newest blob of its type).
    std::unordered_map<BlobType, std::reference_wrapper<Entry>> mNewestEntries;
    ::ErpVector mPcrHash;

    class Refresher;
    std::unique_ptr<Refresher> mRefresher;

    void rebuildCache (void);
    template<class KeyType, class ValueType>
    BlobCache::Entry getBlob(const KeyType key, std::unordered_map<KeyType, ValueType>& map,
                             const bool allowInvalidBlobs = false);
};


#endif
