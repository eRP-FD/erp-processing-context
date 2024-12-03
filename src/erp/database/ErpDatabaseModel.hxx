/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ERP_DATABASEMODEL_HXX
#define ERP_PROCESSING_CONTEXT_ERP_DATABASEMODEL_HXX

#include "erp/model/ChargeItem.hxx"
#include "erp/model/Task.hxx"
#include "shared/database/DatabaseCodec.hxx"
#include "shared/database/DatabaseModel.hxx"
#include "shared/hsm/ErpTypes.hxx"
#include "shared/hsm/HsmClient.hxx"
#include "shared/model/AuditData.hxx"
#include "shared/model/Bundle.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/model/TelematikId.hxx"
#include "shared/model/Timestamp.hxx"

#include <string>
#include <vector>

class ErpVector;
class SafeString;
class JWT;

namespace db_model
{

class Task
{
public:
    explicit Task(const model::PrescriptionId& initPrescriptionId, BlobId initKeyBlobId, Blob initSalt,
                  model::Task::Status initStatus, model::Timestamp initLastStatusChanged,
                  const model::Timestamp& initAuthoredOn, const model::Timestamp& initLastModified);

    model::PrescriptionId prescriptionId;
    BlobId blobId;
    Blob salt;
    model::Task::Status status;
    model::Timestamp lastStatusChange;
    model::Timestamp authoredOn;
    model::Timestamp lastModified;
    std::optional<EncryptedBlob> kvnr;
    std::optional<model::Timestamp> expiryDate;
    std::optional<model::Timestamp> acceptDate;
    std::optional<EncryptedBlob> accessCode;
    std::optional<EncryptedBlob> secret;
    std::optional<EncryptedBlob> receipt;
    std::optional<EncryptedBlob> healthcareProviderPrescription;
    std::optional<EncryptedBlob> owner;
    std::optional<model::Timestamp> lastMedicationDispense;
};

class MedicationDispense
{
public:
    explicit MedicationDispense(model::PrescriptionId initPrescriptionId, EncryptedBlob initMedicationDispense,
                                BlobId initKeyBlobId, Blob initSalt);
    model::PrescriptionId prescriptionId;
    EncryptedBlob medicationDispense;
    BlobId blobId;
    Blob salt;
};

class Communication
{
public:
    Uuid id;
    EncryptedBlob communication;
    std::optional<model::Timestamp> received;
    BlobId blobId{};
    Blob salt;
};

struct ChargeItem {
    explicit ChargeItem(const ::model::PrescriptionId& id);

    ChargeItem(const ::model::ChargeInformation& chargeInformation, const ::BlobId& newBlobId, const Blob& newSalt,
               const ::SafeString& key, const ::DataBaseCodec& codec);

    [[nodiscard]] ::model::ChargeInformation
    toChargeInformation(const std::optional<DatabaseCodecWithKey>& codecAndKey) const;

    ::model::PrescriptionId prescriptionId;
    EncryptedBlob enterer = {};
    ::model::Timestamp enteredDate = ::model::Timestamp::now();
    ::model::Timestamp lastModified = ::model::Timestamp::now();
    ::std::optional<Blob> markingFlags = {};
    BlobId blobId = {};
    Blob salt = {};
    EncryptedBlob accessCode = {};
    EncryptedBlob kvnr = {};
    // The following are optional because they are not set when updating a charge item.
    // They will always exist in the database, though.
    ::std::optional<EncryptedBlob> prescription = {};
    ::std::optional<EncryptedBlob> prescriptionJson = {};
    ::std::optional<EncryptedBlob> receiptXml = {};
    ::std::optional<EncryptedBlob> receiptJson = {};
    EncryptedBlob billingData = {};
    EncryptedBlob billingDataJson = {};
};

class AccessTokenIdentity
{
public:
    explicit AccessTokenIdentity(const JWT& jwt);
    AccessTokenIdentity(const model::TelematikId& id, std::string_view name, std::string_view oid);

    std::string getJson() const;
    const model::TelematikId& getId() const;
    const std::string& getName() const;
    const std::string& getOid() const;

    static AccessTokenIdentity fromJson(const std::string& json);

private:
    model::TelematikId mId;
    std::string mName;
    std::string mOid;
};

}// namespace db_model


#endif// ERP_PROCESSING_CONTEXT_ERP_DATABASEMODEL_HXX
