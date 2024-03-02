/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "test/mock/MockVsdmKeyBlobDatabase.hxx"


MockVsdmKeyBlobDatabase::Entry MockVsdmKeyBlobDatabase::getBlob(char operatorId, char version) const
{
    std::lock_guard lock(mMutex);

    auto entry = find(operatorId, version);
    ErpExpect(entry != std::end(mEntries), HttpStatus::NotFound, "Blob does not exist");
    return *entry;
}

std::vector<MockVsdmKeyBlobDatabase::Entry> MockVsdmKeyBlobDatabase::getAllBlobs() const
{
    std::lock_guard lock(mMutex);
    return mEntries;
}


void MockVsdmKeyBlobDatabase::storeBlob(Entry&& entry)
{
    std::lock_guard lock(mMutex);
    ErpExpect(find(entry.operatorId, entry.version) == std::end(mEntries), HttpStatus::Conflict,
              "blob with the same name already exists");

    mEntries.push_back(entry);
}

void MockVsdmKeyBlobDatabase::deleteBlob(char operatorId, char version)
{
    std::lock_guard lock(mMutex);
    auto entry = find(operatorId, version);
    ErpExpect(entry != std::end(mEntries), HttpStatus::NotFound, "Blob does not exist");
    mEntries.erase(entry);
}


std::vector<MockVsdmKeyBlobDatabase::Entry>::const_iterator MockVsdmKeyBlobDatabase::find(char operatorId,
                                                                                          char version) const
{
    return std::find_if(cbegin(mEntries), cend(mEntries), [&](const auto& entry) {
        return entry.operatorId == operatorId && entry.version == version;
    });
}

void MockVsdmKeyBlobDatabase::recreateConnection()
{
}
