/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_DATABASEMODEL_HXX
#define ERP_PROCESSING_CONTEXT_DATABASEMODEL_HXX

#include "erp/hsm/ErpTypes.hxx"
#include "erp/hsm/HsmClient.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/model/AuditData.hxx"

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
    static HashedKvnr fromKvnr(const std::string_view& kvnr, const SafeString& persistencyIndexKey);

private:
    explicit HashedKvnr(HashedId&& hashedKvnr);
};

class HashedTelematikId: public HashedId
{
public:
    static HashedTelematikId fromTelematikId(const std::string_view& id, const SafeString& persistencyIndexKey);
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

class ChargeItem {
public:
    ChargeItem(model::PrescriptionId initPrescriptionId, BlobId initBlobId, Blob initSalt,
                      model::Timestamp initAuthoredOn, db_model::EncryptedBlob initChargeItem);

    model::PrescriptionId prescriptionId;
    BlobId blobId;
    Blob salt;
    model::Timestamp authoredOn;
    db_model::EncryptedBlob chargeItem;
};

} // namespace db_model






#endif // ERP_PROCESSING_CONTEXT_DATABASEMODEL_HXX
