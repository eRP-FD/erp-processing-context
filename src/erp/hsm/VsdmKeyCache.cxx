/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/hsm/VsdmKeyCache.hxx"
#include "erp/enrolment/VsdmHmacKey.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/TLog.hxx"

namespace
{
std::string makeCacheKey(char operatorId, char version)
{
    std::string key{operatorId};
    key += version;
    return key;
}
}// namespace

class VsdmKeyCache::Refresher : public FixedIntervalHandler
{
public:
    explicit Refresher(VsdmKeyCache& keyCache, std::chrono::steady_clock::duration interval)
        : FixedIntervalHandler{interval}
        , mKeyCache{keyCache}
    {
    }

private:
    void timerHandler() override
    {
        TLOG(INFO) << "Refreshing VSDM Key Cache.";
        try
        {
            mKeyCache.rebuildCache();
        }
        catch (const std::runtime_error& error)
        {
            TLOG(ERROR) << "failed to refresh the VSDM Key cache: " << error.what();
        }
    }
    VsdmKeyCache& mKeyCache;
};


VsdmKeyCache::VsdmKeyCache(std::unique_ptr<VsdmKeyBlobDatabase> db, HsmPool& hsmPool)
    : mDatabase{std::move(db)}
    , mHsmPool{hsmPool}
{
    rebuildCache();
}

void VsdmKeyCache::rebuildCache()
{
    const auto dbEntries = mDatabase->getAllBlobs();
    std::unordered_map<std::string, SafeString> newCache;
    newCache.reserve(dbEntries.size());
    {
        std::shared_lock lock(mCacheMutex);
        for (const auto& dbEntry : dbEntries)
        {
            const auto key = makeCacheKey(dbEntry.operatorId, dbEntry.version);
            const auto it = mEntryCache.find(key);
            // If there is already a unwrapped entry in our current db, reuse it
            // which saves us the call to the HSM.
            if (it != mEntryCache.end())
            {
                newCache.emplace(key, it->second);
            }
            else
            {
                // in case of a new entry, unwrap it using the HSM
                try
                {
                    auto hsmPoolSession = mHsmPool.acquire();
                    auto& hsmSession = hsmPoolSession.session();
                    ErpVector unwrappedEntry = hsmSession.unwrapRawPayload(dbEntry.blob);
                    std::string_view payload{reinterpret_cast<const char*>(unwrappedEntry.data()), unwrappedEntry.size()};
                    VsdmHmacKey vsdmKeyData{payload};
                    newCache.emplace(key, SafeString{Base64::decode(vsdmKeyData.plainTextKey())});
                }
                catch(const std::exception& e)
                {
                    TLOG(ERROR) << "Failed to add VSDM key for operator '" << dbEntry.operatorId << "' version '"
                                << dbEntry.version << "': " << e.what();
                }
            }
        }
    }

    std::unique_lock lock{mCacheMutex};
    mEntryCache.swap(newCache);
}


SafeString VsdmKeyCache::getKey(char operatorId, char version)
{
    const auto key = makeCacheKey(operatorId, version);
    std::shared_lock lock(mCacheMutex);
    auto it = mEntryCache.find(key);
    if (it == mEntryCache.end())
    {
        lock.unlock();
        rebuildCache();
        lock.lock();
        it = mEntryCache.find(key);
    }
    ErpExpect(it != mEntryCache.end(), HttpStatus::NotFound,
              "Vsdm hmac key for operator " + std::string{operatorId} + " version " + std::string{version} +
                  " not present in db");
    return it->second;
}

VsdmKeyBlobDatabase& VsdmKeyCache::getVsdmKeyBlobDatabase()
{
    return *mDatabase;
}

void VsdmKeyCache::startRefresher(boost::asio::io_context& context, std::chrono::steady_clock::duration interval)
{
    Expect3(mRefresher == nullptr, "refresher already started", std::logic_error);
    mRefresher = std::make_unique<PeriodicTimer<Refresher>>(*this, interval);
    mRefresher->start(context, interval);
}
