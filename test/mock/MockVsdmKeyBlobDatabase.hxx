/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_MOCK_MOCKVSDMKEYBLOBDATABASE_HXX
#define ERP_PROCESSING_CONTEXT_TEST_MOCK_MOCKVSDMKEYBLOBDATABASE_HXX

#include "src/shared/hsm/VsdmKeyBlobDatabase.hxx"

#include <functional>
#include <mutex>
#include <unordered_map>

/**
 * A mocked version of the VSDM Key blob database where all data is stored only in-memory
 *  and is therefore not persisted.
 */
class MockVsdmKeyBlobDatabase : public VsdmKeyBlobDatabase
{
public:
    Entry getBlob(char operatorId, char version) const override;
    std::vector<Entry> getAllBlobs() const override;

    void storeBlob(Entry&& entry) override;

    void deleteBlob(char operatorId, char version) override;
    void recreateConnection() override;

private:
    std::vector<Entry>::const_iterator find(char operatorId, char version) const;
    mutable std::mutex mMutex;
    std::vector<Entry> mEntries;
};

#endif