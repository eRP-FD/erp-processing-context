/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "DatabaseModel.hxx"
#include "shared/compression/ZStd.hxx"
#include "shared/database/CommonDatabaseFrontend.hxx"
#include "shared/hsm/ErpTypes.hxx"
#include "shared/util/Configuration.hxx"


CommonDatabaseFrontend::CommonDatabaseFrontend(HsmPool& hsmPool, KeyDerivation& keyDerivation)
    : mHsmPool(hsmPool)
    , mDerivation(keyDerivation)
    , mCodec(compressionInstance())
{
}

/* static */ std::shared_ptr<Compression> CommonDatabaseFrontend::compressionInstance()
{
    static auto theCompressionInstance =
        std::make_shared<ZStd>(Configuration::instance().getStringValue(ConfigurationKey::ZSTD_DICTIONARY_DIR));
    return theCompressionInstance;
}

std::tuple<SafeString, BlobId> CommonDatabaseFrontend::auditEventKey(DatabaseBackend& backend,
                                                                     const db_model::HashedKvnr& hashedKvnr)
{
    auto blobId = mHsmPool.acquire().session().getLatestAuditLogPersistenceId();
    auto salt = backend.retrieveSaltForAccount(hashedKvnr, db_model::MasterKeyType::auditevent, blobId);

    if (salt)
    {
        return {mDerivation.auditEventKey(hashedKvnr, blobId, *salt), blobId};
    }
    else
    {
        SafeString keyForAuditData;
        OptionalDeriveKeyData secondCallData;
        std::tie(keyForAuditData, secondCallData) = mDerivation.initialAuditEventKey(hashedKvnr);
        auto dbSalt =
            backend.insertOrReturnAccountSalt(hashedKvnr, db_model::MasterKeyType::auditevent, secondCallData.blobId,
                                               db_model::Blob{std::vector<uint8_t>{secondCallData.salt}});
        // there was a concurrent insert so we need to derive again with the
        // salt created by the concurrent, who was first to insert the salt.
        blobId = secondCallData.blobId;
        if (dbSalt)
        {
            return {mDerivation.auditEventKey(hashedKvnr, secondCallData.blobId, *dbSalt), blobId};
        }
        return {std::move(keyForAuditData), blobId};
    }
}

std::string CommonDatabaseFrontend::storeAuditEventData(DatabaseBackend& backend,
                                                        model::AuditData& auditData)
{
    auto hashedKvnr = mDerivation.hashKvnr(auditData.insurantKvnr());

    std::optional<db_model::EncryptedBlob> encryptedMeta;
    std::optional<BlobId> auditEventBlobId;
    if (! auditData.metaData().isEmpty())
    {
        auto [keyForAuditData, blobId] = auditEventKey(backend, hashedKvnr);
        encryptedMeta = mCodec.encode(auditData.metaData().serializeToJsonString(), keyForAuditData,
                                      Compression::DictionaryUse::Default_json);
        auditEventBlobId = blobId;
    }

    db_model::AuditData dbAuditData(auditData.agentType(), auditData.eventId(), std::move(encryptedMeta),
                                    auditData.action(), std::move(hashedKvnr), auditData.deviceId(),
                                    auditData.prescriptionId(), auditEventBlobId);

    auto id = backend.storeAuditEventData(dbAuditData);
    auditData.setId(id);
    auditData.setRecorded(dbAuditData.recorded);
    return id;
}
