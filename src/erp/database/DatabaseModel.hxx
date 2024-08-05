/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_DATABASEMODEL_HXX
#define ERP_PROCESSING_CONTEXT_DATABASEMODEL_HXX

#include "erp/database/DatabaseCodec.hxx"
#include "erp/hsm/ErpTypes.hxx"
#include "erp/hsm/HsmClient.hxx"
#include "erp/model/AuditData.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Kvnr.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/TelematikId.hxx"
#include "erp/model/Timestamp.hxx"

#include <string>
#include <vector>

class ErpVector;
class SafeString;

namespace db_model
{

using postgres_bytea_view = std::basic_string_view<std::byte>;
using postgres_bytea = std::basic_string<std::byte>;

enum class MasterKeyType: int8_t
{
    medicationDispense = 1,
    communication = 2,
    auditevent = 3,
};

class Blob: public std::vector<std::byte>
{
public:
    using std::vector<std::byte>::vector;
    Blob() = default;
    explicit Blob(std::vector<std::byte>&&);
    explicit Blob(const postgres_bytea_view& pqxxBin);
    explicit Blob(const std::vector<uint8_t>& vec);
    void append(const std::string_view& str);
    postgres_bytea_view binarystring() const;
    std::string toHex() const;
};

class EncryptedBlob : public Blob
{
public:
    using Blob::Blob;
    EncryptedBlob() = default;
    explicit EncryptedBlob(std::vector<std::byte>&&);
};

class HashedId: public EncryptedBlob
{
public:
    explicit HashedId(EncryptedBlob&& hashedId);
};


class HashedKvnr: public HashedId
{
public:
    static HashedKvnr fromKvnr(const model::Kvnr& kvnr, const SafeString& persistencyIndexKey);

private:
    explicit HashedKvnr(HashedId&& hashedKvnr);
};

class HashedTelematikId: public HashedId
{
public:
    static HashedTelematikId fromTelematikId(const model::TelematikId& id, const SafeString& persistencyIndexKey);
private:
    explicit HashedTelematikId(HashedId&& id);
};


class Task
{
public:
    explicit Task(const model::PrescriptionId& initPrescriptionId,
                  BlobId initKeyBlobId,
                  Blob initSalt,
                  model::Task::Status initStatus,
                  const model::Timestamp& initAuthoredOn,
                  const model::Timestamp& initLastModified);

    model::PrescriptionId prescriptionId;
    BlobId blobId;
    Blob salt;
    model::Task::Status status;
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
    explicit MedicationDispense(model::PrescriptionId initPrescriptionId,
                                EncryptedBlob initMedicationDispense,
                                BlobId initKeyBlobId,
                                Blob initSalt);
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

class AuditData
{
public:
    AuditData(model::AuditEvent::AgentType agentType, model::AuditEventId eventId, std::optional<EncryptedBlob> metaData,
              model::AuditEvent::Action action, HashedKvnr insurantKvnr, int16_t deviceId,
              std::optional<model::PrescriptionId> prescriptionId, std::optional<BlobId> blobId);

    model::AuditEvent::AgentType agentType;
    model::AuditEventId eventId;
    std::optional<EncryptedBlob> metaData;
    model::AuditEvent::Action action;
    HashedKvnr insurantKvnr;
    std::int16_t deviceId;
    std::optional<model::PrescriptionId> prescriptionId;
    std::optional<BlobId> blobId;

    std::string id;            // filled after storing in or if loaded from DB;
    model::Timestamp recorded; // filled after storing in or if loaded from DB;
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

}// namespace db_model


#endif// ERP_PROCESSING_CONTEXT_DATABASEMODEL_HXX
