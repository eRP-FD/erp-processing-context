/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/database/DatabaseModel.hxx"
#include "shared/crypto/Pbkdf2Hmac.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/Hash.hxx"

#include <algorithm>

using namespace db_model;
using namespace ::std::literals;

postgres_bytea_view db_model::Blob::binarystring() const
{
    return {data(), size()};
}

std::string db_model::Blob::toHex() const
{
    gsl::span<const char> span(reinterpret_cast<const char*>(data()), size());
    return ByteHelper::toHex(span);
}

db_model::Blob::Blob(std::vector<std::byte> && blob)
    : vector<std::byte>(std::move(blob))
{
}

db_model::Blob::Blob(const postgres_bytea_view& pqxxBin)
    : vector<std::byte>(pqxxBin.begin(), pqxxBin.end())
{
}

Blob::Blob(const std::vector<uint8_t>& vec)
    : vector<std::byte>(reinterpret_cast<const std::byte*>(vec.data()),
                        reinterpret_cast<const std::byte*>(vec.data()) + vec.size())
{
}

void Blob::append(const std::string_view& str)
{
    reserve(str.size() + size());
    insert(end(), reinterpret_cast<const std::byte*>(str.data()),
           reinterpret_cast<const std::byte*>(str.data()) + str.size());
}

db_model::EncryptedBlob::EncryptedBlob(std::vector<std::byte> &&encryptedBlob)
    : Blob(std::move(encryptedBlob))
{
}

HashedId::HashedId(EncryptedBlob&& hashedId)
    : EncryptedBlob{std::move(hashedId)}
{}

 db_model::HashedId HashedId::fromString(const std::string& s)
{
    const auto hashedStr = Hash::sha256(s);
    std::vector<std::byte> buf(hashedStr.size());
    std::transform(hashedStr.begin(), hashedStr.end(), buf.begin(), [](char c) { return std::byte(c); });
    return db_model::HashedId{std::move(buf)};
}

HashedKvnr::HashedKvnr(HashedId&& hashedKvnr)
    : HashedId(std::move(hashedKvnr))
{
}

HashedKvnr HashedKvnr::fromKvnr(const model::Kvnr& kvnr,
                                const SafeString& persistencyIndexKey)
{
    ErpExpect(kvnr.validFormat(), HttpStatus::BadRequest, "Invalid KVNR");

    return HashedKvnr{
        db_model::HashedId(db_model::EncryptedBlob(Pbkdf2Hmac::deriveKey(kvnr.id(), persistencyIndexKey)))};
}

HashedTelematikId HashedTelematikId::fromTelematikId(const model::TelematikId& id, const SafeString& persistencyIndexKey)
{
    return HashedTelematikId{
        db_model::HashedId(db_model::EncryptedBlob(Pbkdf2Hmac::deriveKey(id.id(), persistencyIndexKey)))};
}

HashedTelematikId::HashedTelematikId(HashedId&& id)
    : HashedId{std::move(id)}
{}

AuditData::AuditData(model::AuditEvent::AgentType agentType, model::AuditEventId eventId,
                     std::optional<EncryptedBlob> metaData, model::AuditEvent::Action action, HashedKvnr insurantKvnr,
                     int16_t deviceId, std::optional<model::PrescriptionId> prescriptionId,
                     std::optional<BlobId> blobId)
    : agentType(agentType)
    , eventId(eventId)
    , metaData(std::move(metaData))
    , action(action)
    , insurantKvnr(std::move(insurantKvnr))
    , deviceId(deviceId)
    , prescriptionId(std::move(prescriptionId))
    , blobId(blobId)
    , recorded(model::Timestamp::now())
{
}

PushEventData::PushEventData(std::string&& notificationIdentifier, model::ChannelId channelId, db_model::HashedKvnr&& hashedKvnr, std::vector<model::PrescriptionId>&& prescriptions)
    : mNotificationIdentifier(std::move(notificationIdentifier))
    , mChannelId(channelId)
    , mHashedKvnr(std::move(hashedKvnr))
    , mPrescriptions(std::move(prescriptions))
{
}

const std::string& PushEventData::notificationIdentifier() const
{
    return mNotificationIdentifier;
}

const model::ChannelId& PushEventData::channelId() const
{
    return mChannelId;
}

const db_model::HashedKvnr& PushEventData::hashedKvnr() const
{
    return mHashedKvnr;
}

const std::vector<model::PrescriptionId>& PushEventData::prescriptions() const
{
    return mPrescriptions;
}
