/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MOCKACCOUNTTABLE_HXX
#define ERP_PROCESSING_CONTEXT_MOCKACCOUNTTABLE_HXX

#include "erp/database/DatabaseModel.hxx"
#include "erp/hsm/HsmClient.hxx"


class MockAccountTable {
public:
    std::optional<db_model::Blob> insertOrReturnAccountSalt(const db_model::HashedId& accountId,
                                                            db_model::MasterKeyType masterKeyType,
                                                            BlobId blobId,
                                                            const db_model::Blob& salt);
    std::optional<db_model::Blob> getSalt(const db_model::HashedId& accountId,
                                          db_model::MasterKeyType masterKeyType,
                                          BlobId blobId);

private:
    struct AccountsPrimaryKey
    {
        db_model::HashedId accountId;
        db_model::MasterKeyType masterKeyType;
        BlobId blobId;
        bool operator < (const AccountsPrimaryKey& rhs) const;
    private:
        auto tie() const -> decltype(std::tie(accountId, masterKeyType, blobId));
    };

    std::map<AccountsPrimaryKey, db_model::Blob> mAccounts; ///< AccountsPrimaryKey -> Salt

};

#endif// ERP_PROCESSING_CONTEXT_MOCKACCOUNTTABLE_HXX
