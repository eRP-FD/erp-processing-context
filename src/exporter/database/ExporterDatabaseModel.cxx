/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/database/ExporterDatabaseModel.hxx"

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
                     const std::optional<EncryptedBlob>& pharmacyIdentity)
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
{
}