/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "erp/database/push/PushErpBackend.hxx"
#include "shared/database/CommonPostgresBackend.hxx"
#include "shared/database/PostgresConnection.hxx"


class PushErpPostgresBackend : public CommonPostgresBackend, public PushErpBackend
{
public:
    enum class ColumnName
    {
        pushkey_hashed,
        app_id_hashed,
        kvnr_hashed,
        blob_id,
        salt,
        payload,
        url,
        encryption_key,
        subscribed_channel,
        time_created,
        last_modified
    };

    explicit PushErpPostgresBackend(PostgresConnection& connection);

    void healthCheck() override;
    PostgresConnection& connection() const override;

    std::optional<db_model::Pusher> findRegistration(const db_model::HashedId& hashedPushKey, const db_model::HashedId& hashedAppId,
                                                     const db_model::HashedKvnr& kvnr) const override;
    std::vector<db_model::Pusher> getRegistrations(const db_model::HashedKvnr& kvnr) const override;
    void upsertRegistration(const db_model::Pusher& pusher, const db_model::EncryptionKey& encryptionKey) override;
    void deleteRegistration(const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey,
                            const db_model::HashedId& hashedAppId) override;

    void updateChannels(const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey,
                        const db_model::HashedId& hashedAppId,
                        const model::Channels& channels) override;
    std::pair<std::basic_string<std::byte>, std::optional<model::Channels>>
    getChannels(const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey) const override;
    bool isPushRegistered(const db_model::HashedKvnr& hashedKvnr, const model::ChannelId& channelId) override;

private:
    static db_model::Pusher createPusher(const db_model::HashedKvnr& kvnr, const pqxx::row& resultRow);

    static model::Channels channelsFromRow(const pqxx::row& row);

    PostgresConnection& mConnection;
};
