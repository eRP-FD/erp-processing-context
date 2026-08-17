/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/database/DatabaseModel.hxx"
#include "shared/model/HashedKvnr.hxx"
#include "shared/model/push/Pusher.hxx"

namespace db_model
{
struct Pusher {
    HashedId hashedPushKey;
    HashedId hashedAppId;
    HashedKvnr kvnr;
    BlobId blobId;
    Blob salt;
    EncryptedBlob payload;
    std::string url;
    model::Timestamp lastModified;
};
struct EncryptionKey {
    EncryptedBlob encryptionKey;
    model::Timestamp timeCreated;
};
}
