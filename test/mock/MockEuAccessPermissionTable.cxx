// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "MockEuAccessPermissionTable.hxx"


bool MockEuAccessPermissionTable::existsCountryCode(const std::string& countryCode)
{
    return mCountries.contains(countryCode);
}

void MockEuAccessPermissionTable::deleteEuAccessPermission(const db_model::HashedKvnr& kvnr)
{
    mRows.erase(kvnr);
}

void MockEuAccessPermissionTable::createEuAccessPermission(const db_model::HashedKvnr& kvnr,
                                                           const std::string& countryCode,
                                                           const db_model::EncryptedBlob& accessCode, BlobId blobId,
                                                           const db_model::Blob& salt,
                                                           const model::Timestamp& validUntil)
{
    mRows.insert_or_assign(kvnr, Row{kvnr, countryCode, accessCode, blobId, salt, validUntil});
}

std::optional<db_model::EuAccessPermission>
MockEuAccessPermissionTable::retrieveEuAccessPermission(const db_model::HashedKvnr& kvnr)
{
    if (mRows.contains(kvnr))
    {
        return db_model::EuAccessPermission{mRows.at(kvnr).countryCode, mRows.at(kvnr).accessCode,
                                            mRows.at(kvnr).validUntil, mRows.at(kvnr).blobId, mRows.at(kvnr).salt};
    }
    return std::nullopt;
}

void MockEuAccessPermissionTable::addCountry(const std::string& country)
{
    mCountries.insert(country);
}
