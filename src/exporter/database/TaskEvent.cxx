/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <utility>

#include "exporter/database/TaskEvent.hxx"

using namespace db_model;

TaskEvent::TaskEvent(id_t id, const model::PrescriptionId& prescriptionId, std::int16_t prescriptionType,
                     BlobId keyBlobId, db_model::Blob salt, const db_model::EncryptedBlob& kvnr,
                     const HashedKvnr& hashedKvnr, const std::string& state, const std::string& usecase,
                     model::Timestamp lastModified, model::Timestamp authoredOn,
                     const std::optional<EncryptedBlob>& healthcareProviderPrescription,
                     const std::optional<BlobId>& medicationDispenseBundleBlobId,
                     const std::optional<Blob>& medicationDispenseSalt,
                     const std::optional<EncryptedBlob>& medicationDispenseBundle,
                     const std::optional<EncryptedBlob>& doctorIdentity,
                     const std::optional<EncryptedBlob>& pharmacyIdentity,
                     std::int32_t retryCount)
    : id(id)
    , prescriptionId(prescriptionId)
    , prescriptionType(prescriptionType)
    , blobId(keyBlobId)
    , salt(std::move(salt))
    , kvnr(kvnr)
    , hashedKvnr(hashedKvnr)
    , state(state)
    , usecase(usecase)
    , lastModified(lastModified)
    , authoredOn(authoredOn)
    , healthcareProviderPrescription(healthcareProviderPrescription)
    , medicationDispenseBundleBlobId(medicationDispenseBundleBlobId)
    , medicationDispenseSalt(medicationDispenseSalt)
    , medicationDispenseBundle(medicationDispenseBundle)
    , doctorIdentity(doctorIdentity)
    , pharmacyIdentity(pharmacyIdentity)
    , retryCount(retryCount)
{
}

PushEvent::PushEvent(id_t id, HashedKvnr kvnrHashed, model::PrescriptionId prescriptionId, std::string channelId,
                     std::string notificationIdentifier, std::int32_t retryCount, model::Timestamp created)
    : id(id)
    , kvnrHashed(std::move(kvnrHashed))
    , prescriptionId(prescriptionId)
    , channelId(std::move(channelId))
    , notificationIdentifier(std::move(notificationIdentifier))
    , retryCount(retryCount)
    , created(created)
{
}

AppRegistration::AppRegistration(HashedId pushKeyHashed, HashedId appIdHashed, HashedKvnr kvnrHashed, BlobId blobId,
                                 Blob salt, EncryptedBlob payload, std::string url, EncryptedBlob encryptionKey,
                                 model::Timestamp timeCreated, model::Timestamp lastModified)
    : pushKeyHashed(std::move(pushKeyHashed))
    , appIdHashed(std::move(appIdHashed))
    , kvnrHashed(std::move(kvnrHashed))
    , blobId(blobId)
    , salt(std::move(salt))
    , payload(std::move(payload))
    , url(std::move(url))
    , encryptionKey(std::move(encryptionKey))
    , timeCreated(timeCreated)
    , lastModified(lastModified)
{
}
