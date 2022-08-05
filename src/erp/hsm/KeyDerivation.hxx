/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_KEYDERIVATION_HXX
#define ERP_PROCESSING_CONTEXT_KEYDERIVATION_HXX

#include "erp/hsm/HsmClient.hxx"
#include "erp/util/SafeString.hxx"

#include <mutex>
#include <tuple>


class ErpVector;
class HsmPool;
struct OptionalDeriveKeyData;

namespace db_model
{
class Blob;
class HashedId;
class HashedKvnr;
}
namespace fhirtools{
class Timestamp;
}

namespace model
{
using fhirtools::Timestamp;
class PrescriptionId;
}

namespace db_model
{
class HashedId;
class HashedKvnr;
class HashedTelematikId;
}

class KeyDerivation
{
public:
    explicit KeyDerivation(HsmPool& hsmPool);

    [[nodiscard]] SafeString taskKey(const model::PrescriptionId& taskId,
                                     const fhirtools::Timestamp& authoredOn,
                                     BlobId blobId,
                                     const db_model::Blob& salt);

    [[nodiscard]] std::tuple<SafeString, OptionalDeriveKeyData> initialTaskKey(const model::PrescriptionId& taskId,
                                                                        const fhirtools::Timestamp& authoredOn);

    [[nodiscard]] db_model::HashedKvnr hashKvnr(std::string_view kvnr) const;
    [[nodiscard]] db_model::HashedTelematikId hashTelematikId(std::string_view tid) const;

    /// @brief create a HashedId from indentity string
    /// Autodetects if it is KVNR or TelematikId
    db_model::HashedId hashIdentity(std::string_view identity) const;



    [[nodiscard]]
    SafeString medicationDispenseKey(const db_model::HashedKvnr& kvnr,
                                     BlobId blobId,
                                     const db_model::Blob& salt);

    [[nodiscard]]
    std::tuple<SafeString, OptionalDeriveKeyData> initialMedicationDispenseKey(const db_model::HashedKvnr& kvnr);

    [[nodiscard]]
    std::tuple<SafeString, OptionalDeriveKeyData>
    initialCommunicationKey(const std::string_view& identity, const db_model::HashedId& IdentityHashed);

    [[nodiscard]]
    SafeString communicationKey(const std::string_view& identity,
                                const db_model::HashedId& identityHashed,
                                BlobId blobId,
                                const db_model::Blob& salt);

    [[nodiscard]] SafeString auditEventKey(const db_model::HashedKvnr& kvnr, BlobId blobId,
                                           const db_model::Blob& salt);

    [[nodiscard]] std::tuple<SafeString, OptionalDeriveKeyData> initialAuditEventKey(const db_model::HashedKvnr& kvnr);
    
    [[nodiscard]] ::SafeString chargeItemKey(const ::model::PrescriptionId& taskId, ::BlobId blobId,
                                             const ::db_model::Blob& salt);

    [[nodiscard]] ::std::tuple<::SafeString, ::OptionalDeriveKeyData>
    initialChargeItemKey(const ::model::PrescriptionId& prescriptionId);

    KeyDerivation(const KeyDerivation&) = delete;
    KeyDerivation(KeyDerivation&&) = delete;
    KeyDerivation& operator = (const KeyDerivation&) = delete;
    KeyDerivation& operator = (KeyDerivation&&) = delete;

private:
    [[nodiscard]] static ErpVector taskKeyDerivationData(const model::PrescriptionId& taskId,
                                                         const fhirtools::Timestamp& authoredOn);
    [[nodiscard]] static ErpVector medicationDispenseKeyDerivationData(const db_model::HashedKvnr& kvnr);
    [[nodiscard]] static ErpVector auditEventKeyDerivationData(const db_model::HashedKvnr& kvnr);
    [[nodiscard]] static ErpVector communicationKeyDerivationData(const std::string_view& identity,
                                                                  const db_model::HashedId& identityHashed);

    const SafeString& getPersistenceIndexKeyKvnr (void) const;
    const SafeString& getPersistenceIndexKeyTelematikId (void) const;

    HsmPool& mHsmPool;
    mutable std::optional<SafeString> mPersistenceIndexKeyKvnr;
    mutable std::optional<SafeString> mPersistenceIndexKeyTelematikId;
    mutable std::mutex mPersistenceIndexKeyMutex;
};


#endif// ERP_PROCESSING_CONTEXT_KEYDERIVATION_HXX
