#include "erp/database/DatabaseModel.hxx"
#include "erp/crypto/Pbkdf2Hmac.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/Gsl.hxx"

#include <algorithm>

using namespace db_model;

pqxx::binarystring db_model::Blob::binarystring() const
{
    return pqxx::binarystring{data(), size()};
}

std::string db_model::Blob::toHex() const
{
    gsl::span<const char> span(reinterpret_cast<const char*>(data()), size());
    return ByteHelper::toHex(span);
}

db_model::Blob::Blob(std::vector<uint8_t> && blob)
    : vector<uint8_t>(std::move(blob))
{
}

db_model::Blob::Blob(const pqxx::binarystring& pqxxBin)
    : vector<uint8_t>(pqxxBin.begin(), pqxxBin.end())
{
}

db_model::EncryptedBlob::EncryptedBlob(std::vector<uint8_t> &&encryptedBlob)
    : Blob(std::move(encryptedBlob))
{
}

HashedId::HashedId(EncryptedBlob&& hashedId)
    : EncryptedBlob{std::move(hashedId)}
{}

HashedKvnr::HashedKvnr(HashedId&& hashedKvnr)
    : HashedId(std::move(hashedKvnr))
{
}

HashedKvnr HashedKvnr::fromKvnr(const std::string_view& kvnr,
                                const SafeString& persistencyIndexKey)
{
    ErpExpect(kvnr.find('\0') == std::string::npos, HttpStatus::BadRequest, "null character in kvnr");
    ErpExpect(kvnr.size() == 10, HttpStatus::BadRequest, "kvnr must have 10 characters");

    return HashedKvnr{
        db_model::HashedId(db_model::EncryptedBlob(Pbkdf2Hmac::deriveKey(kvnr, persistencyIndexKey)))};
}

HashedTelematikId HashedTelematikId::fromTelematikId(const std::string_view& id, const SafeString& persistencyIndexKey)
{
    return HashedTelematikId{
        db_model::HashedId(db_model::EncryptedBlob(Pbkdf2Hmac::deriveKey(id, persistencyIndexKey)))};
}

HashedTelematikId::HashedTelematikId(HashedId&& id)
    : HashedId{std::move(id)}
{}

db_model::Task::Task(const model::PrescriptionId& initPrescriptionId,
                     BlobId initKeyBlobId,
                     db_model::Blob initSalt,
                     model::Task::Status initStatus,
                     const model::Timestamp& initAuthoredOn,
                     const model::Timestamp& initLastModified)
    : prescriptionId(initPrescriptionId)
    , blobId(initKeyBlobId)
    , salt(std::move(initSalt))
    , status(initStatus)
    , authoredOn(initAuthoredOn)
    , lastModified(initLastModified)
{

}

MedicationDispense::MedicationDispense(model::PrescriptionId initPrescriptionId,
                                       db_model::EncryptedBlob initMedicationDispense,
                                       BlobId initKeyBlobId,
                                       db_model::Blob initSalt)
    : prescriptionId(std::move(initPrescriptionId))
    , medicationDispense(std::move(initMedicationDispense))
    , blobId(initKeyBlobId)
    , salt(std::move(initSalt))
{
}

AuditData::AuditData(model::AuditEvent::AgentType agentType, model::AuditEventId eventId,
                     std::optional<EncryptedBlob> metaData, model::AuditEvent::Action action, HashedKvnr insurantKvnr,
                     int16_t deviceId, std::optional<model::PrescriptionId> prescriptionId, std::optional<BlobId> blobId)
    : agentType(agentType)
    , eventId(eventId)
    , metaData(std::move(metaData))
    , action(action)
    , insurantKvnr(std::move(insurantKvnr))
    , deviceId(deviceId)
    , prescriptionId(std::move(prescriptionId))
    , blobId(blobId)
    , id()
    , recorded(model::Timestamp::now())
{
}
