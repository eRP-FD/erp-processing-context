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
    auto [account, inserted] = mAccounts.try_emplace({accountId, masterKeyType, blobId}, salt);
    return inserted?std::nullopt:std::make_optional(account->second);
}

std::optional<db_model::Blob> MockAccountTable::getSalt(const db_model::HashedId& accountId,
                                                        db_model::MasterKeyType masterKeyType,
                                                        BlobId blobId)
{
    auto account = mAccounts.find({accountId, masterKeyType, blobId});
    return (account == mAccounts.end())?std::nullopt:std::make_optional(account->second);
}

bool MockAccountTable::isBlobUsed(BlobId blobId) const
{
    auto hasBlobId = [blobId](const auto& row) { return row.first.blobId == blobId;};
    auto blobUser = find_if(mAccounts.begin(), mAccounts.end(), hasBlobId);
    if (blobUser != mAccounts.end())
    {
        TVLOG(0) << "Blob " << blobId << " is still in use by an account";
        return true;
    }
    return false;
}
