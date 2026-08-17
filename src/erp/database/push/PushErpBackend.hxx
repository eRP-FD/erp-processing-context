/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "erp/database/push/PushDbModel.hxx"
#include "shared/database/DatabaseBackend.hxx"
#include "shared/util/Buffer.hxx"

#include <optional>


namespace model
{
enum class ChannelId : uint8_t;
}
namespace model
{
class Channels;
}
class PushErpBackend : virtual public DatabaseBackend
{
public:
    virtual std::optional<db_model::Pusher> findRegistration(const db_model::HashedId& hashedPushKey,
                                                             const db_model::HashedId& hashedAppId,
                                                             const db_model::HashedKvnr& kvnr) const = 0;
    virtual std::vector<db_model::Pusher> getRegistrations(const db_model::HashedKvnr& kvnr) const = 0;

    virtual void upsertRegistration(const db_model::Pusher& pusher, const db_model::EncryptionKey& encryptionKey) = 0;
    virtual void deleteRegistration(const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey,
                                    const db_model::HashedId& hashedAppId) = 0;

    virtual void updateChannels(const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey,
                                const db_model::HashedId& hashedAppId,
                                const model::Channels& channels) = 0;
    virtual std::pair<std::basic_string<std::byte>, std::optional<model::Channels>>
    getChannels(const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey) const = 0;
    virtual bool isPushRegistered(const db_model::HashedKvnr& hashedKvnr, const model::ChannelId& channelId) = 0;
};
