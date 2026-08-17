/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/database/MainDatabaseFrontend.hxx"
#include "TaskEvent.hxx"
#include "exporter/model/push/PushNotification.hxx"
#include "exporter/model/push/PushNotificationContext.hxx"
#include "exporter/model/push/PushNotificationEvent.hxx"
#include "shared/audit/AuditDataCollector.hxx"
#include "shared/util/Demangle.hxx"
#include "shared/util/Hash.hxx"
#include "shared/util/JsonLog.hxx"

using namespace exporter;

MainDatabaseFrontend::MainDatabaseFrontend(std::unique_ptr<MainPostgresBackend>&& backend, HsmPool& hsmPool,
                                           KeyDerivation& keyDerivation)
    : mBackend(std::move(backend))
    , mCommonDatabaseFrontend(std::make_unique<CommonDatabaseFrontend>(hsmPool, keyDerivation))
    , mDerivation(keyDerivation)
    , mCodec(CommonDatabaseFrontend::compressionInstance())
{
}

MainDatabaseFrontend::~MainDatabaseFrontend() = default;

std::shared_ptr<Compression> MainDatabaseFrontend::compressionInstance()
{
    return CommonDatabaseFrontend::compressionInstance();
}

std::tuple<SafeString, BlobId> MainDatabaseFrontend::auditEventKey(const db_model::HashedKvnr& hashedKvnr)
{
    return mCommonDatabaseFrontend->auditEventKey(*mBackend, hashedKvnr);
}

std::string MainDatabaseFrontend::storeAuditEventData(const AuditDataCollector& auditDataCollector)
{
    try
    {
        model::AuditData auditData = auditDataCollector.createData();
        return mCommonDatabaseFrontend->storeAuditEventData(*mBackend, auditData);
    }
    catch (const MissingAuditDataException& exc)
    {
        TVLOG(2) << "Missing audit data";
        JsonLog(LogId::INFO, JsonLog::makeErrorLogReceiver(), false).details("Missing audit data");
    }
    catch (const std::exception& exc)
    {
        // Could be an I/O error.
        const auto typeinfo = util::demangle(typeid(exc).name());
        TVLOG(1) << "Error while storing audit data: " << typeinfo;
        TVLOG(1) << "Error reason:  " << exc.what();
    }
    return {};
}

void MainDatabaseFrontend::healthCheck()
{
    mBackend->healthCheck();
}

std::optional<DatabaseConnectionInfo> MainDatabaseFrontend::getConnectionInfo() const
{
    return mBackend->getConnectionInfo();
}

void MainDatabaseFrontend::commitTransaction()
{
    mBackend->commitTransaction();
}

// GEMREQ-start A_27652
std::vector<model::PushNotificationContext>
MainDatabaseFrontend::retrievePushNotificationEventData(const model::PushNotificationEvent& pushNotificationEvent)
{
    auto appRegistrations = mBackend->retrieveAppRegistrations(pushNotificationEvent.hashedKvnr(),
                                                               pushNotificationEvent.pushNotification().channelId());
    std::vector<model::PushNotificationContext> pushNotificationEvents;
    for (const auto& appRegistration : appRegistrations)
    {
        auto key = mDerivation.appRegistrationKey(appRegistration.pushKeyHashed, appRegistration.salt,
                                                  pushNotificationEvent.hashedKvnr(), appRegistration.blobId);
        auto payload = mCodec.decode(appRegistration.payload, key);
        auto pusher = model::Pusher::fromDb(std::string{payload.c_str()});
        model::PushNotificationDevice device{pusher.appId().value, pusher.pushKey().value,
                                             std::chrono::duration_cast<std::chrono::seconds>(
                                                 appRegistration.lastModified.toChronoTimePoint().time_since_epoch()),
                                             pusher.pusherData(), pusher.deviceDisplayName()};
        auto encryptionKey = model::EncryptionKey::deserializeCsv(mCodec.decode(appRegistration.encryptionKey, key));

        pushNotificationEvents.emplace_back(pushNotificationEvent.pushNotification(), std::move(device),
                                            std::move(encryptionKey), encryptionKey.getKeyIdentifier(),
                                            pushNotificationEvent.created(), appRegistration.salt,
                                            appRegistration.blobId, pushNotificationEvent.id(),
                                            pushNotificationEvent.hashedKvnr(), pushNotificationEvent.retryCount(),
                                            pushNotificationEvent.auditOrCommunicationIdentifier());
    }
    return pushNotificationEvents;
}
// GEMREQ-end A_27652

void MainDatabaseFrontend::updateEncryptionKey(const db_model::HashedKvnr& kvnrHashed,
                                               const model::PushNotificationContext& pushNotificationEvent)
{
    const auto hashedPushKey = db_model::HashedId::fromString(pushNotificationEvent.device().pushkey());
    const auto hashedAppId = db_model::HashedId::fromString(pushNotificationEvent.device().appId());

    const auto key = mDerivation.appRegistrationKey(hashedPushKey,
                                                    pushNotificationEvent.salt(), kvnrHashed, pushNotificationEvent.blobId());
    const auto encryptionKey = mCodec.encode(pushNotificationEvent.encryptionKey().serializeToCsv(pushNotificationEvent.keyIdentifier()), key,
                                             Compression::DictionaryUse::Default_json);
    mBackend->updateEncryptionKey(kvnrHashed, hashedPushKey, hashedAppId, encryptionKey);
}

void MainDatabaseFrontend::deletePushKey(const model::HashedKvnr& hashedKvnr, const model::PushKey& pushKey) const
{
    const auto hashedPushKey = db_model::HashedId::fromString(pushKey.value);
    mBackend->deletePushKey(hashedKvnr.toDbModel(), hashedPushKey);
}
