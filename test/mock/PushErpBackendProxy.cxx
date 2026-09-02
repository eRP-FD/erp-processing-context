/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/mock/PushErpBackendProxy.hxx"

#include "erp/model/push/Channels.hxx"

#include <optional>

PushErpBackendProxy::PushErpBackendProxy(std::shared_ptr<PushErpBackend> backend)
    : mBackend{std::move(backend)}
{
}

void PushErpBackendProxy::commitTransaction()
{
    mBackend->commitTransaction();
}


void PushErpBackendProxy::closeConnection()
{
    mBackend->closeConnection();
}

bool PushErpBackendProxy::isCommitted() const
{
    return mBackend->isCommitted();
}

std::string PushErpBackendProxy::retrieveSchemaVersion()
{
    return mBackend->retrieveSchemaVersion();
}

void PushErpBackendProxy::healthCheck()
{
    mBackend->healthCheck();
}

std::optional<DatabaseConnectionInfo> PushErpBackendProxy::getConnectionInfo() const
{
    return mBackend->getConnectionInfo();
}

std::string PushErpBackendProxy::storeAuditEventData(db_model::AuditData& auditData)
{
    return mBackend->storeAuditEventData(auditData);
}

std::optional<db_model::Blob>
PushErpBackendProxy::retrieveSaltForAccount(const db_model::HashedId& accountId,
                                            db_model::MasterKeyType masterKeyType, BlobId blobId)
{
    return mBackend->retrieveSaltForAccount(accountId, masterKeyType, blobId);
}

std::optional<db_model::Blob>
PushErpBackendProxy::insertOrReturnAccountSalt(const db_model::HashedId& accountId,
                                               db_model::MasterKeyType masterKeyType, BlobId blobId,
                                               const db_model::Blob& salt)
{
    return mBackend->insertOrReturnAccountSalt(accountId, masterKeyType, blobId, salt);
}

std::optional<db_model::Pusher> PushErpBackendProxy::findRegistration(const db_model::HashedId& hashedPushKey,
                                                                      const db_model::HashedId& hashedAppId,
                                                                      const db_model::HashedKvnr& kvnr) const
{
    return mBackend->findRegistration(hashedPushKey, hashedAppId, kvnr);
}

std::vector<db_model::Pusher> PushErpBackendProxy::getRegistrations(const db_model::HashedKvnr& kvnr) const
{
    return mBackend->getRegistrations(kvnr);
}

void PushErpBackendProxy::upsertRegistration(const db_model::Pusher& pusher,
                                             const db_model::EncryptionKey& encryptionKey)
{
    mBackend->upsertRegistration(pusher, encryptionKey);
}

void PushErpBackendProxy::deleteRegistration(const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey,
                                             const db_model::HashedId& hashedAppId)
{
    mBackend->deleteRegistration(kvnr, hashedPushKey, hashedAppId);
}

void PushErpBackendProxy::updateChannels(const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey,
                                         const db_model::HashedId& hashedAppId, const model::Channels& channels)
{
    mBackend->updateChannels(kvnr, hashedPushKey, hashedAppId, channels);
}

std::pair<std::basic_string<std::byte>, std::optional<model::Channels>>
PushErpBackendProxy::getChannels(const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey) const
{
    return mBackend->getChannels(kvnr, hashedPushKey);
}

bool PushErpBackendProxy::isPushRegistered(const db_model::HashedKvnr& hashedKvnr, const model::ChannelId& channelId)
{
    return mBackend->isPushRegistered(hashedKvnr, channelId);
}

