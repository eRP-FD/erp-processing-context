/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "erp/database/push/PushErpBackend.hxx"

class PushErpBackendProxy : public PushErpBackend
{
public:
    PushErpBackendProxy(std::shared_ptr<PushErpBackend> backend);
    void commitTransaction() override;
    void closeConnection() override;
    bool isCommitted() const override;

    std::string retrieveSchemaVersion() override;

    void healthCheck() override;

    std::optional<DatabaseConnectionInfo> getConnectionInfo() const override;

    std::string storeAuditEventData(db_model::AuditData& auditData) override;
    [[nodiscard]]
    std::optional<db_model::Blob> retrieveSaltForAccount(const db_model::HashedId& accountId,
                                                         db_model::MasterKeyType masterKeyType, BlobId blobId) override;

    [[nodiscard]]
    std::optional<db_model::Blob> insertOrReturnAccountSalt(const db_model::HashedId& accountId,
                                                            db_model::MasterKeyType masterKeyType, BlobId blobId,
                                                            const db_model::Blob& salt) override;


    std::optional<db_model::Pusher> findRegistration(const db_model::HashedId& hashedPushKey,
                                                     const db_model::HashedId& hashedAppId,
                                                     const db_model::HashedKvnr& kvnr) const override;
    std::vector<db_model::Pusher> getRegistrations(const db_model::HashedKvnr& kvnr) const override;

    void upsertRegistration(const db_model::Pusher& pusher, const db_model::EncryptionKey& encryptionKey) override;
    void deleteRegistration(const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey,
                            const db_model::HashedId& hashedAppId) override;

    void updateChannels(const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey,
                        const db_model::HashedId& hashedAppId, const model::Channels& channels) override;
    std::pair<std::basic_string<std::byte>, std::optional<model::Channels>>
    getChannels(const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey) const override;
    bool isPushRegistered(const db_model::HashedKvnr& hashedKvnr, const model::ChannelId& channelId) override;

private:
    std::shared_ptr<PushErpBackend> mBackend;
};
