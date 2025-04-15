/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_MOCK_HSM_MOCKBLOBDATABASE_HXX
#define ERP_MOCK_HSM_MOCKBLOBDATABASE_HXX

#include "shared/hsm/BlobDatabase.hxx"
#include "mock/hsm/MockBlobCache.hxx"

#include <mutex>
#include <unordered_map>

#if WITH_HSM_MOCK  != 1
#error MockBlobDatabase.hxx included but WITH_HSM_MOCK not enabled
#endif

/**
 * A mocked version of the blob database where all data is stored only in-memory and is therefore not persisted.
 */
class MockBlobDatabase : public BlobDatabase
{
public:
    Entry getBlob (
        BlobType type,
        BlobId id) const override;
    Entry getBlob(BlobType type, const ErpVector& name) const override;
    std::vector<Entry> getAllBlobsSortedById (void) const override;

    BlobId storeBlob (Entry&& entry) override;

    void deleteBlob (BlobType type, const ErpVector& name) override;

    std::vector<bool> hasValidBlobsOfType (std::vector<BlobType>&& blobTypes) const override;

    void setBlobInUseTest(std::function<bool(BlobId blobId)> isBlobInUseFunction);

    static std::shared_ptr<BlobCache> createBlobCache(const MockBlobCache::MockTarget target);

    void recreateConnection() override;

private:
    mutable std::mutex mMutex;
    BlobId mNextBlobId = 0;
    std::unordered_map<ErpVector, Entry> mEntries;
    std::function<bool(BlobId)> mIsBlobInUse;

    BlobId getNextBlobId(std::lock_guard<std::mutex>&);
};

#endif
