/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_MOCK_HSM_MOCKBLOBDATABASE_HXX
#define ERP_MOCK_HSM_MOCKBLOBDATABASE_HXX

#include "erp/hsm/BlobDatabase.hxx"

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
    virtual Entry getBlob (
        BlobType type,
        BlobId id) const override;
    virtual std::vector<Entry> getAllBlobsSortedById (void) const override;

    virtual BlobId storeBlob (Entry&& entry) override;

    virtual void deleteBlob (BlobType type, const ErpVector& name) override;

    virtual std::vector<bool> hasValidBlobsOfType (std::vector<BlobType>&& blobTypes) const override;

private:
    mutable std::mutex mMutex;
    std::unordered_map<ErpVector, Entry> mEntries;

    BlobId getNextBlobId (BlobType type) const;
};

#endif
