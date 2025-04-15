/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MOCKACCOUNTTABLE_HXX
#define ERP_PROCESSING_CONTEXT_MOCKACCOUNTTABLE_HXX

#include "erp/database/ErpDatabaseModel.hxx"

#include <map>


class MockAccountTable {
public:
    std::optional<db_model::Blob> insertOrReturnAccountSalt(const db_model::HashedId& accountId,
                                                            db_model::MasterKeyType masterKeyType,
                                                            BlobId blobId,
                                                            const db_model::Blob& salt);
    std::optional<db_model::Blob> getSalt(const db_model::HashedId& accountId,
                                          db_model::MasterKeyType masterKeyType,
                                          BlobId blobId);

    bool isBlobUsed(BlobId blobId) const;

private:
    struct AccountsPrimaryKey
    {
        db_model::HashedId accountId;
        db_model::MasterKeyType masterKeyType{};
        BlobId blobId{};
        bool operator < (const AccountsPrimaryKey& rhs) const;
    private:
        auto tie() const -> decltype(std::tie(accountId, masterKeyType, blobId));
    };

    std::map<AccountsPrimaryKey, db_model::Blob> mAccounts; ///< AccountsPrimaryKey -> Salt

};

#endif// ERP_PROCESSING_CONTEXT_MOCKACCOUNTTABLE_HXX
