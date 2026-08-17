/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "erp/database/push/PushErpBackend.hxx"
#include "erp/model/push/Channels.hxx"

#undef Expect
#include <gmock/gmock.h>


class PushErpMockBackend : public PushErpBackend
{
public:
    MOCK_METHOD(void, commitTransaction, (), (override));
    MOCK_METHOD(void, closeConnection, (), (override));
    MOCK_METHOD(bool, isCommitted, (), (const, override));
    MOCK_METHOD(std::string, retrieveSchemaVersion, (), (override));
    MOCK_METHOD(void, healthCheck, (), (override));
    MOCK_METHOD(std::string, storeAuditEventData, (db_model::AuditData & auditData), (override));
    MOCK_METHOD(std::optional<db_model::Blob>, retrieveSaltForAccount,
                (const db_model::HashedId& accountId, db_model::MasterKeyType masterKeyType, BlobId blobId),
                (override));
    MOCK_METHOD(std::optional<db_model::Blob>, insertOrReturnAccountSalt,
                (const db_model::HashedId& accountId, db_model::MasterKeyType masterKeyType, BlobId blobId,
                 const db_model::Blob& salt),
                (override));
    MOCK_METHOD(std::optional<db_model::Pusher>, findRegistration,
                (const db_model::HashedId& hashedPushKey, const db_model::HashedId& hashedAppId, const db_model::HashedKvnr& kvnr),
                (const, override));
    MOCK_METHOD(std::vector<db_model::Pusher>, getRegistrations, (const db_model::HashedKvnr& kvnr), (const, override));
    MOCK_METHOD(void, upsertRegistration,
                (const db_model::Pusher& pusher, const db_model::EncryptionKey& encryptionKey), (override));
    MOCK_METHOD(void, deleteRegistration,
                (const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey, const db_model::HashedId& hashedAppId),
                (override));
    MOCK_METHOD(void, updateChannels,
                (const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey, const db_model::HashedId& hashedAppId, const model::Channels& channels),
                (override));
    MOCK_METHOD((std::pair<std::basic_string<std::byte>, std::optional<model::Channels>>), getChannels,
                (const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey), (const, override));
    MOCK_METHOD(bool, isPushRegistered, (const db_model::HashedKvnr& hashedKvnr, const model::ChannelId& channelId),
                (override));
};
