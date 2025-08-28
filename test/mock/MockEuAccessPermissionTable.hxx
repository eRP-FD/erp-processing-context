// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef MOCKEUACCESSPERMISSIONTABLE_HXX
#define MOCKEUACCESSPERMISSIONTABLE_HXX
#include "erp/database/ErpDatabaseModel.hxx"
#include "shared/database/DatabaseModel.hxx"

#include <map>
#include <set>

class MockEuAccessPermissionTable
{
public:
    bool existsCountryCode(const std::string& countryCode);
    void deleteEuAccessPermission(const db_model::HashedKvnr& kvnr);
    void createEuAccessPermission(const db_model::HashedKvnr& kvnr, const std::string& countryCode,
                                  const db_model::EncryptedBlob& accessCode, BlobId blobId,
                                  const db_model::Blob& salt, const model::Timestamp& validUntil);
    std::optional<db_model::EuAccessPermission> retrieveEuAccessPermission(const db_model::HashedKvnr& kvnr);
    void addCountry(const std::string& country);
private:
    struct Row {
        db_model::HashedKvnr kvnrHashed;
        std::string countryCode;
        db_model::EncryptedBlob accessCode;
        BlobId blobId;
        db_model::Blob salt;
        model::Timestamp validUntil;
    };

    std::map<db_model::HashedKvnr, Row> mRows;

    std::set<std::string> mCountries;
};


#endif//MOCKEUACCESSPERMISSIONTABLE_HXX
