/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_VSDMKEYCACHE_HXX
#define ERP_PROCESSING_CONTEXT_VSDMKEYCACHE_HXX

#include "shared/hsm/VsdmKeyBlobDatabase.hxx"
#include "shared/util/PeriodicTimer.hxx"

#include <memory>
#include <shared_mutex>
#include <unordered_map>

class HsmPool;

/**
 * Implements a read-only vsdm key cache. This cache stores the decrypted
 * VSDM HMAC keys to avoid calls to the database and HSM to obtain and
 * unwrap the vsdm hmac keys.
 *
 */
class VsdmKeyCache
{
public:
    explicit VsdmKeyCache(std::unique_ptr<VsdmKeyBlobDatabase> db, HsmPool& hsmPool);

    /**
     * Obtain the VSDM HMAC key for the given operator and version.
     * If the key is not present, the cache is updated automatically; if the key is
     * still not to be found in the database, an ErpExeption is thrown.
     */
    SafeString getKey(char operatorId, char version);

    void startRefresher(boost::asio::io_context& context, std::chrono::steady_clock::duration interval);

    VsdmKeyBlobDatabase& getVsdmKeyBlobDatabase();

private:
    void rebuildCache();

    std::shared_mutex mCacheMutex{};
    std::unordered_map<std::string, SafeString> mEntryCache;
    std::unique_ptr<VsdmKeyBlobDatabase> mDatabase{};
    HsmPool& mHsmPool;

    class Refresher;
    std::unique_ptr<PeriodicTimerBase> mRefresher{};

#ifdef FRIEND_TEST
    FRIEND_TEST(VsdmKeyCacheTest, keyRemoval);
    FRIEND_TEST(VsdmKeyCacheTest, skipFailingUnwrap);
#endif
};


#endif// ERP_PROCESSING_CONTEXT_VSDMKEYCACHE_HXX
