/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/push/PushErpDatabase.hxx"
#include "erp/database/push/PushExporterDatabaseException.hxx"
#include "erp/model/push/Channels.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/database/CommonDatabaseFrontend.hxx"
#include "shared/model/push/EncryptionKey.hxx"
#include "shared/util/Hash.hxx"
#include "shared/util/String.hxx"

#include <fmt/format.h>
#include <ranges>

PushErpDatabase::PushErpDatabase(std::unique_ptr<PushErpBackend>&& backend, HsmPool& hsmPool,
                                 KeyDerivation& keyDerivation)
    : mBackend(std::move(backend))
    , mCommonDatabaseFrontend(std::make_unique<CommonDatabaseFrontend>(hsmPool, keyDerivation))
    , mDerivation(keyDerivation)
    , mCodec{CommonDatabaseFrontend::compressionInstance()}
{
}

std::optional<model::Pusher> PushErpDatabase::findRegistration(const model::PushKey& pushKey, const model::AppId& appId,
                                                               const model::Kvnr& kvnr) const
{
    // Throws when pushKey or appId do not match with the pushKey and appId from the encrypted payload of the db entry.
    return findRegistration(pushKey, appId, db_model::HashedId::fromString(appId.value), kvnr);
}

std::vector<model::Pusher> PushErpDatabase::getRegistrations(const model::Kvnr& kvnr) const
{
    const auto hashedKvnr = mDerivation.hashKvnr(kvnr);
    std::vector<model::Pusher> ret;
    const auto dbModel = mBackend->getRegistrations(hashedKvnr);
    if (! dbModel.empty())
    {
        ret.reserve(dbModel.size());
        for (const auto& dbPusher : dbModel)
        {
            ret.emplace_back(fromDbModel(dbPusher));
        }
    }
    return ret;
}

void PushErpDatabase::createOrUpdateRegistration(const model::Kvnr& kvnr, const model::Pusher& pusher)
{
    const auto hashedKvnr = mDerivation.hashKvnr(kvnr);
    const auto hashedPushKey = db_model::HashedId::fromString(pusher.pushKey().value);
    const auto [key, derivationData] = mDerivation.initialAppRegistrationKey(hashedPushKey, hashedKvnr);
    mBackend->upsertRegistration(toDbModel(hashedKvnr, pusher, key, derivationData),
                                 toEncryptionKeyDbModel(pusher, key));
}

void PushErpDatabase::deleteRegistration(const model::Kvnr& kvnr, const model::PushKey& pushKey,
                                         const model::AppId& appId)
{
    mBackend->deleteRegistration(mDerivation.hashKvnr(kvnr), db_model::HashedId::fromString(pushKey.value), db_model::HashedId::fromString(appId.value));
}

void PushErpDatabase::updateChannels(const model::Kvnr& kvnr, const model::PushKey& pushkey,
                                     const db_model::HashedId& hashedAppId,
                                     const model::Channels& channels)
{
    mBackend->updateChannels(mDerivation.hashKvnr(kvnr), db_model::HashedId::fromString(pushkey.value), hashedAppId, channels);
}

std::pair<db_model::HashedId, std::optional<model::Channels>> PushErpDatabase::getChannels(const model::Kvnr& kvnr,
                                                            const model::PushKey& pushkey) const
{
    auto [hashedAppIdBytes, channels] = mBackend->getChannels(mDerivation.hashKvnr(kvnr), db_model::HashedId::fromString(pushkey.value));

    const auto hashedAppId = db_model::HashedId(std::move(hashedAppIdBytes));

    // Throws when pushKey or appId do not match with the pushKey and appId from the encrypted payload of the db entry.
    findRegistration(pushkey, {}, hashedAppId, kvnr);

    return {hashedAppId, channels};
}

bool PushErpDatabase::isPushRegistered(const db_model::HashedKvnr& hashedKvnr,
                                       const model::ChannelId& channelId)
{
    try
    {
        const bool isRegistered = push::erp::exporter::error::withDatabaseErrorHandling("committing database transaction.", [&]
        {
            return mBackend->isPushRegistered(hashedKvnr, channelId);
        });
        return isRegistered;
    }
    catch (const PushErpExporterDatabaseException& exc)
    {
        TLOG(WARNING) << exc.what();
    }
    return false;
}

std::string PushErpDatabase::storeAuditEventData(model::AuditData& auditData)
{
    return mCommonDatabaseFrontend->storeAuditEventData(*mBackend, auditData);
}

PushErpBackend& PushErpDatabase::getBackend() const
{
    return *mBackend;
}

void PushErpDatabase::commitTransaction()
{
    mBackend->commitTransaction();
}

db_model::Pusher PushErpDatabase::toDbModel(const db_model::HashedKvnr& kvnr, const model::Pusher& pusher,
                                            const SafeString& key, const OptionalDeriveKeyData& derivationData) const
{
    A_28674.start("App-Registrierung - Zeitstempel speichern");
    const auto now = model::Timestamp::now();
    const auto hashedPushKey = db_model::HashedId::fromString(pusher.pushKey().value);
    const auto hashedAppId = db_model::HashedId::fromString(pusher.appId().value);
    return {.hashedPushKey = hashedPushKey,
            .hashedAppId = hashedAppId,
            .kvnr = kvnr,
            .blobId = derivationData.blobId,
            .salt = db_model::Blob{derivationData.salt},
            .payload = mCodec.encode(pusher.dbPayloadJson(), key, Compression::DictionaryUse::Default_json),
            .url = pusher.pusherData().mUrl,
            .lastModified = now};
}

db_model::EncryptionKey PushErpDatabase::toEncryptionKeyDbModel(const model::Pusher& pusher,
                                                                const SafeString& key) const
{
    // GEMREQ-start A_27157
    A_27157.start("FdV-Instanz registrieren – Initiale Schlüsselableitung");
    const auto initialDerivation =
        model::EncryptionKey::deriveMonthlyKey(pusher.encryption().iss, pusher.encryption().timeIssCreated);
    auto initialDerivationCsv = initialDerivation.serializeToCsv(pusher.encryption().keyIdentifier);
    return {
        .encryptionKey = mCodec.encode(initialDerivationCsv, key, Compression::DictionaryUse::Default_json),
        .timeCreated = model::Timestamp::now()// also on update, the initial key derivation is overwritten on update.
    };
    // GEMREQ-end A_27157
}

model::Pusher PushErpDatabase::fromDbModel(const db_model::Pusher& dbModel) const
{
    const auto key = mDerivation.appRegistrationKey(dbModel.hashedPushKey, dbModel.salt, dbModel.kvnr, dbModel.blobId);
    return model::Pusher::fromDb(std::string{std::string_view{mCodec.decode(dbModel.payload, key)}});
}

std::optional<model::Pusher> PushErpDatabase::findRegistration(const model::PushKey& pushKey,
                                                               const std::optional<model::AppId>& appId,
                                                               const db_model::HashedId& hashedAppId,
                                                               const model::Kvnr& kvnr) const
{
    const auto hashedKvnr = mDerivation.hashKvnr(kvnr);
    const auto dbModel = mBackend->findRegistration(db_model::HashedId::fromString(pushKey.value), hashedAppId, hashedKvnr);
    if (dbModel.has_value())
    {
        auto result = fromDbModel(*dbModel);
        const bool check1 = pushKey.value == result.pushKey().value;
        const int check2 = appId.has_value() ? static_cast<int>(appId->value == result.appId().value) : 2;
        const bool ok = check1 && (check2 != 0);
        std::string diag;
        if (! check1)
        {
            diag += "pushKey ";
        }
        if (check2 == 0)
        {
            diag += "appId ";
        }
        ErpExpect(ok, HttpStatus::BadRequest,
                  fmt::format("Mismatch in {}- kvnr: {} pushkey: {} appid: {}", diag, hashedKvnr.toHex(),
                              dbModel->hashedPushKey.toHex(),
                              (check2 == 2) ? "skipped" : dbModel->hashedAppId.toHex()));
        return result;
    }
    return std::nullopt;
}
