/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "erp/database/push/PushErpBackend.hxx"
#include "shared/database/CommonDatabaseFrontend.hxx"
#include "shared/database/DatabaseCodec.hxx"

#include <functional>
#include <memory>
#include <optional>


namespace model
{
class EncryptionKey;
class Channels;
class Kvnr;
class Pusher;
struct PushKey;
struct AppId;
}
class HsmPool;
class KeyDerivation;

class PushErpReadOnlyDatabase
{
public:
    using Factory = std::function<std::unique_ptr<PushErpReadOnlyDatabase>(HsmPool&, KeyDerivation&)>;

    virtual ~PushErpReadOnlyDatabase() = default;

    virtual bool isPushRegistered(const db_model::HashedKvnr& hashedKvnr, const model::ChannelId& channelId) = 0;
};

class PushErpDatabase : public PushErpReadOnlyDatabase
{
public:
    using Factory = std::function<std::unique_ptr<PushErpDatabase>(HsmPool&, KeyDerivation&)>;

    PushErpDatabase(std::unique_ptr<PushErpBackend>&& backend, HsmPool& hsmPool, KeyDerivation& keyDerivation);

    std::optional<model::Pusher> findRegistration(const model::PushKey& pushKey, const model::AppId& appId,
                                                  const model::Kvnr& kvnr) const;
    std::vector<model::Pusher> getRegistrations(const model::Kvnr& kvnr) const;

    void createOrUpdateRegistration(const model::Kvnr& kvnr, const model::Pusher& pusher);
    void deleteRegistration(const model::Kvnr& kvnr, const model::PushKey& pushKey, const model::AppId& appId);

    void updateChannels(const model::Kvnr& kvnr, const model::PushKey& pushkey, const db_model::HashedId& hashedAppId,
                        const model::Channels& channels);
    std::pair<db_model::HashedId, std::optional<model::Channels>> getChannels(const model::Kvnr& kvnr,
                                                                              const model::PushKey& pushkey) const;

    bool isPushRegistered(const db_model::HashedKvnr& hashedKvnr, const model::ChannelId& channelId) override;

    std::string storeAuditEventData(model::AuditData& auditData);

    PushErpBackend& getBackend() const;
    void commitTransaction();

private:
    db_model::Pusher toDbModel(const db_model::HashedKvnr& kvnr, const model::Pusher& pusher, const SafeString& key,
                               const OptionalDeriveKeyData& derivationData) const;
    db_model::EncryptionKey toEncryptionKeyDbModel(const model::Pusher& pusher, const SafeString& key) const;
    model::Pusher fromDbModel(const db_model::Pusher& dbModel) const;
    std::optional<model::Pusher> findRegistration(const model::PushKey& pushKey,
                                                  const std::optional<model::AppId>& appId,
                                                  const db_model::HashedId& hashedAppId, const model::Kvnr& kvnr) const;

    std::unique_ptr<PushErpBackend> mBackend;
    std::unique_ptr<CommonDatabaseFrontend> mCommonDatabaseFrontend;
    KeyDerivation& mDerivation;
    DataBaseCodec mCodec;
};
