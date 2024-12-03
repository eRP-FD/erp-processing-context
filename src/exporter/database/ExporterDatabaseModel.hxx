/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_EXPORTER_DATABASEMODEL_HXX
#define ERP_EXPORTER_DATABASEMODEL_HXX

#include "shared/database/DatabaseCodec.hxx"
#include "shared/database/DatabaseModel.hxx"
#include "shared/hsm/ErpTypes.hxx"
#include "shared/hsm/HsmClient.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/model/Timestamp.hxx"

namespace db_model
{

class TaskEvent
{
public:
    using id_t = std::int64_t;

    explicit TaskEvent(id_t id, const model::PrescriptionId& prescriptionId, std::int16_t prescriptionType,
                       BlobId keyBlobId, Blob salt, const EncryptedBlob& kvnr, const HashedKvnr& hashedKvnr,
                       const std::string& state, const std::string& usecase, model::Timestamp lastModified, model::Timestamp authoredOn,
                       const std::optional<EncryptedBlob>& healthcareProviderPrescription,
                       const std::optional<BlobId>& medicationDispenseBundleBlobId,
                       const std::optional<Blob>& medicationDispenseSalt,
                       const std::optional<EncryptedBlob>& medicationDispenseBundle,
                       const std::optional<EncryptedBlob>& doctorIdentity,
                       const std::optional<EncryptedBlob>& pharmacyIdentity);

    id_t id;
    model::PrescriptionId prescriptionId;
    std::int16_t prescriptionType;
    BlobId blobId;
    Blob salt;
    EncryptedBlob kvnr;
    HashedKvnr hashedKvnr;
    std::string state;
    std::string usecase;
    model::Timestamp lastModified;
    model::Timestamp authoredOn;
    std::optional<EncryptedBlob> healthcareProviderPrescription;
    std::optional<BlobId> medicationDispenseBundleBlobId;
    std::optional<Blob> medicationDispenseSalt;
    std::optional<EncryptedBlob> medicationDispenseBundle;
    std::optional<EncryptedBlob> doctorIdentity;
    std::optional<EncryptedBlob> pharmacyIdentity;
    std::optional<EncryptedBlob> owner;
    std::optional<model::Timestamp> lastMedicationDispense;
};


}// namespace db_model


#endif// ERP_EXPORTER_DATABASEMODEL_HXX
