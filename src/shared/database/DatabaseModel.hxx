/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_DATABASEMODEL_HXX
#define ERP_PROCESSING_CONTEXT_DATABASEMODEL_HXX

#include "shared/database/DatabaseCodec.hxx"
#include "shared/hsm/ErpTypes.hxx"
#include "shared/hsm/HsmClient.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/model/TelematikId.hxx"
#include "shared/model/AuditData.hxx"

#include <string>
#include <vector>

class ErpVector;
class SafeString;

namespace db_model
{

enum class MasterKeyType : int8_t
{
    medicationDispense = 1,
    communication = 2,
    auditevent = 3,
};

using postgres_bytea_view = std::basic_string_view<std::byte>;
using postgres_bytea = std::basic_string<std::byte>;


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
    using EncryptedBlob::EncryptedBlob;
    explicit HashedId(EncryptedBlob&& hashedId);
};


class HashedKvnr: public HashedId
{
public:
    using HashedId::HashedId;
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

class AuditData
{
public:
    AuditData(model::AuditEvent::AgentType agentType, model::AuditEventId eventId,
              std::optional<EncryptedBlob> metaData, model::AuditEvent::Action action, HashedKvnr insurantKvnr,
              int16_t deviceId, std::optional<model::PrescriptionId> prescriptionId, std::optional<BlobId> blobId);

    model::AuditEvent::AgentType agentType;
    model::AuditEventId eventId;
    std::optional<EncryptedBlob> metaData;
    model::AuditEvent::Action action;
    HashedKvnr insurantKvnr;
    std::int16_t deviceId;
    std::optional<model::PrescriptionId> prescriptionId;
    std::optional<BlobId> blobId;

    std::string id;           // filled after storing in or if loaded from DB;
    model::Timestamp recorded;// filled after storing in or if loaded from DB;
};

}// namespace db_model


#endif// ERP_PROCESSING_CONTEXT_DATABASEMODEL_HXX
