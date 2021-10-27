/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/mock/MockAccountTable.hxx"


auto MockAccountTable::AccountsPrimaryKey::tie() const
-> decltype(std::tie(accountId, masterKeyType, blobId))
{
    return std::tie(accountId, masterKeyType, blobId);
}

bool MockAccountTable::AccountsPrimaryKey::operator < (const AccountsPrimaryKey& rhs) const
{
    return tie() < rhs.tie();
}

std::optional<db_model::Blob> MockAccountTable::insertOrReturnAccountSalt(const db_model::HashedId& accountId,
                                                                      db_model::MasterKeyType masterKeyType,
                                                                      BlobId blobId,
                                                                      const db_model::Blob& salt)
{
    auto [account, inserted] = mAccounts.try_emplace({accountId, masterKeyType, blobId}, std::move(salt));
    return inserted?std::nullopt:std::make_optional(account->second);
}

std::optional<db_model::Blob> MockAccountTable::getSalt(const db_model::HashedId& accountId,
                                                        db_model::MasterKeyType masterKeyType,
                                                        BlobId blobId)
{
    auto account = mAccounts.find({accountId, masterKeyType, blobId});
    return (account == mAccounts.end())?std::nullopt:std::make_optional(account->second);
}
