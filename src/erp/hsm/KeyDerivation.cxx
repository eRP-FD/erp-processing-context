/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/hsm/KeyDerivation.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/database/DatabaseModel.hxx"
#include "erp/hsm/ErpTypes.hxx"
#include "erp/hsm/HsmClient.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/model/PrescriptionType.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/model/Kvnr.hxx"
#include "erp/model/TelematikId.hxx"
#include "erp/util/SafeString.hxx"

#include <boost/endian/conversion.hpp>
#include <date/date.h>

KeyDerivation::KeyDerivation(HsmPool& hsmPool)
    : mHsmPool(hsmPool)
    , mPersistenceIndexKeyKvnr()
    , mPersistenceIndexKeyTelematikId()
    , mPersistenceIndexKeyMutex()
{
}

ErpVector KeyDerivation::taskKeyDerivationData(const model::PrescriptionId& taskId, const model::Timestamp& authoredOn)
{
    A_19700.start("Assemble derivation data for task.");
    ErpVector derivationData;
    auto taskIdBe = boost::endian::native_to_big(taskId.toDatabaseId());
    static_assert(sizeof(taskIdBe) == sizeof(taskId.toDatabaseId()), "possible dataloss in endianness conversion.");
    derivationData.insert(derivationData.end(), reinterpret_cast<const unsigned char*>(&taskIdBe),
                          reinterpret_cast<const unsigned char*>(&taskIdBe) + sizeof(taskIdBe));
    static_assert(sizeof(taskId.type()) == 1, "size of prescriptionType has changed");
    derivationData.push_back(std::underlying_type_t<model::PrescriptionType>(taskId.type()));
    // the standard leaves a little room for time types, we want to make sure
    // that this doesn't change with std library updates:
    using Duration = std::chrono::duration<int64_t, std::ratio<1>>;
    using TimePoint = std::chrono::time_point<std::chrono::system_clock, Duration>;
    using std::chrono::time_point_cast;
    static const TimePoint epoch = time_point_cast<Duration>((date::sys_days(date::year{1970} / date::January / 1)));
    Duration authoredOnSinceEpoch = time_point_cast<Duration>(authoredOn.toChronoTimePoint()) - epoch;
    auto authoredOnBe = boost::endian::native_to_big(authoredOnSinceEpoch.count());
    static_assert(sizeof(authoredOnBe) == sizeof(int64_t));
    derivationData.insert(derivationData.end(), reinterpret_cast<const unsigned char*>(&authoredOnBe),
                          reinterpret_cast<const unsigned char*>(&authoredOnBe) + sizeof(authoredOnBe));
    A_19700.finish();
    return derivationData;
}

SafeString KeyDerivation::taskKey(const model::PrescriptionId& taskId,
                                  const model::Timestamp& authoredOn,
                                  BlobId blobId,
                                  const db_model::Blob& salt)
{
    A_19700.start("key derivation for Task.");
    OptionalDeriveKeyData secondCallData;
    secondCallData.blobId = blobId;
    secondCallData.salt.append(salt);
    auto hsmPoolSession = mHsmPool.acquire();
    auto keyData =
        hsmPoolSession.session().deriveTaskPersistenceKey(taskKeyDerivationData(taskId, authoredOn), secondCallData);
    A_19700.finish();
    return SafeString(std::move(keyData.derivedKey)); //NOLINT[hicpp-move-const-arg,performance-move-const-arg]
}

std::tuple<SafeString, OptionalDeriveKeyData> KeyDerivation::initialTaskKey(const model::PrescriptionId& taskId,
                                                                     const model::Timestamp& authoredOn)
{
    A_19700.start("Initial key derivation for Task.");
    auto hsmPoolSession = mHsmPool.acquire();
    auto keyData =
        hsmPoolSession.session().deriveTaskPersistenceKey(taskKeyDerivationData(taskId, authoredOn));
    Expect(keyData.optionalData, "missing salt/blob_id on initial derivation");
    A_19700.finish();
    //NOLINTNEXTLINE[hicpp-move-const-arg,performance-move-const-arg]
    return std::make_tuple(SafeString(std::move(keyData.derivedKey)), std::move(*keyData.optionalData));
}

db_model::HashedKvnr KeyDerivation::hashKvnr(const model::Kvnr& kvnr) const
{
    return db_model::HashedKvnr::fromKvnr(kvnr, getPersistenceIndexKeyKvnr());
}

ErpVector KeyDerivation::medicationDispenseKeyDerivationData(const db_model::HashedKvnr& kvnr)
{
    return ErpVector{kvnr};
}

ErpVector KeyDerivation::auditEventKeyDerivationData(const db_model::HashedKvnr& kvnr)
{
    return ErpVector{kvnr};
}


std::tuple<SafeString, OptionalDeriveKeyData> KeyDerivation::initialMedicationDispenseKey(const db_model::HashedKvnr& kvnr)
{
   A_19700.start("Initial key derivation for medication dispense.");
    auto hsmPoolSession = mHsmPool.acquire();
    auto keyData =
        hsmPoolSession.session().deriveTaskPersistenceKey(medicationDispenseKeyDerivationData(kvnr));
    SafeString key{std::move(keyData.derivedKey)};//NOLINT[hicpp-move-const-arg,performance-move-const-arg]
    Expect(keyData.optionalData, "missing salt/blob_id on initial derivation");
    A_19700.finish();
    return std::make_tuple(std::move(key), std::move(*keyData.optionalData));
}

SafeString KeyDerivation::medicationDispenseKey(const db_model::HashedKvnr& kvnr,
                                                BlobId blobId,
                                                const db_model::Blob& salt)
{
    A_19700.start("key derivation for medication dispense.");
    OptionalDeriveKeyData secondCallData;
    secondCallData.blobId = blobId;
    secondCallData.salt.append(salt);
    ErpVector derivationData{kvnr};
    auto hsmPoolSession = mHsmPool.acquire();
    auto keyData =
        hsmPoolSession.session().deriveTaskPersistenceKey(derivationData, secondCallData);
    SafeString key{std::move(keyData.derivedKey)};//NOLINT[hicpp-move-const-arg,performance-move-const-arg]
    A_19700.finish();
    return key;
}

SafeString KeyDerivation::auditEventKey(const db_model::HashedKvnr& kvnr, BlobId blobId,
                                        const db_model::Blob& salt)
{
    A_19700.start("key derivation for audit event.");
    OptionalDeriveKeyData secondCallData;
    secondCallData.blobId = blobId;
    secondCallData.salt.append(salt);
    ErpVector derivationData{kvnr};
    auto hsmPoolSession = mHsmPool.acquire();
    auto keyData =
        hsmPoolSession.session().deriveAuditLogPersistenceKey(derivationData, secondCallData);
    SafeString key{std::move(keyData.derivedKey)};//NOLINT[hicpp-move-const-arg,performance-move-const-arg]
    A_19700.finish();
    return key;
}
std::tuple<SafeString, OptionalDeriveKeyData> KeyDerivation::initialAuditEventKey(const db_model::HashedKvnr& kvnr)
{
    A_19700.start("Initial key derivation for audit event.");
    auto hsmPoolSession = mHsmPool.acquire();
    auto keyData =
        hsmPoolSession.session().deriveAuditLogPersistenceKey(auditEventKeyDerivationData(kvnr));
    SafeString key{std::move(keyData.derivedKey)};//NOLINT[hicpp-move-const-arg,performance-move-const-arg]
    Expect(keyData.optionalData, "missing salt/key_generation_id on initial derivation");
    A_19700.finish();
    return std::make_tuple(std::move(key), std::move(*keyData.optionalData));
}

db_model::HashedTelematikId KeyDerivation::hashTelematikId(const model::TelematikId& tid) const
{
    return db_model::HashedTelematikId::fromTelematikId(tid, getPersistenceIndexKeyTelematikId());
}

db_model::HashedId KeyDerivation::hashIdentity(std::string_view identity) const
{
    bool isTelematikId = model::TelematikId::isTelematikId(identity);
    if (isTelematikId)
    {
        return hashTelematikId(model::TelematikId{identity});
    }
    else
    {
        return hashKvnr(model::Kvnr{identity});
    }
}

ErpVector KeyDerivation::communicationKeyDerivationData(const std::string_view& identity,
                                                        const db_model::HashedId& identityHashed)
{
    ErpVector data;
    data.reserve(identity.size() + identityHashed.size());
    data.insert(data.end(), identity.begin(), identity.end());
    data.append(identityHashed);
    return data;
}

std::tuple<SafeString, OptionalDeriveKeyData>
KeyDerivation::initialCommunicationKey(const std::string_view& identity, const db_model::HashedId& IdentityHashed)
{
    A_19700.start("initial key derivation for communication.");
    auto hsmPoolSession = mHsmPool.acquire();
    auto keyData = hsmPoolSession.session().deriveCommunicationPersistenceKey(
        communicationKeyDerivationData(identity, IdentityHashed));
    SafeString key{std::move(keyData.derivedKey)};//NOLINT[hicpp-move-const-arg,performance-move-const-arg]
    A_19700.finish();
    return {std::move(key), std::move(*keyData.optionalData)};
}

SafeString KeyDerivation::communicationKey(const std::string_view& identity,
                                           const db_model::HashedId& identityHashed,
                                           BlobId blobId,
                                           const db_model::Blob& salt)
{
    A_19700.start("key derivation for communication.");
    OptionalDeriveKeyData secondCallData;
    secondCallData.blobId = blobId;
    secondCallData.salt.append(salt);
    auto hsmPoolSession = mHsmPool.acquire();
    auto keyData = hsmPoolSession.session().deriveCommunicationPersistenceKey(
        communicationKeyDerivationData(identity, identityHashed), secondCallData);
    SafeString key{std::move(keyData.derivedKey)};//NOLINT[hicpp-move-const-arg,performance-move-const-arg]
    A_19700.finish();
    return key;
}

::SafeString KeyDerivation::chargeItemKey(const ::model::PrescriptionId& prescriptionId, ::BlobId blobId,
                                          const ::db_model::Blob& salt)
{
    A_19700.start("Key derivation for ChargeItem.");
    auto hsmPoolSession = mHsmPool.acquire();

    auto key = hsmPoolSession.session()
                   .deriveChargeItemPersistenceKey(::ErpVector::create(prescriptionId.toString()),
                                                   ::OptionalDeriveKeyData{::ErpVector{salt}, blobId})
                   .derivedKey;
    A_19700.finish();

    return ::SafeString(::std::move(key));//NOLINT[hicpp-move-const-arg,performance-move-const-arg]
}

::std::tuple<::SafeString, ::OptionalDeriveKeyData>
KeyDerivation::initialChargeItemKey(const ::model::PrescriptionId& prescriptionId)
{
    A_19700.start("Initial key derivation for ChargeItem.");
    auto hsmPoolSession = mHsmPool.acquire();
    auto keyData =
        hsmPoolSession.session().deriveChargeItemPersistenceKey(::ErpVector::create(prescriptionId.toString()));
    Expect(keyData.optionalData, "missing salt/blob_id on initial derivation");
    A_19700.finish();

    return ::std::make_tuple(
        ::SafeString{::std::move(keyData.derivedKey)},//NOLINT[hicpp-move-const-arg,performance-move-const-arg]
        ::std::move(*keyData.optionalData));
}

const SafeString& KeyDerivation::getPersistenceIndexKeyKvnr(void) const
{
    std::lock_guard lock(mPersistenceIndexKeyMutex);
    if (! mPersistenceIndexKeyKvnr.has_value())
        mPersistenceIndexKeyKvnr = SafeString(mHsmPool.acquire().session().getKvnrHmacKey());
    return mPersistenceIndexKeyKvnr.value();
}


const SafeString& KeyDerivation::getPersistenceIndexKeyTelematikId (void) const
{
    std::lock_guard lock (mPersistenceIndexKeyMutex);
    if ( ! mPersistenceIndexKeyTelematikId.has_value())
        mPersistenceIndexKeyTelematikId = SafeString(mHsmPool.acquire().session().getTelematikIdHmacKey());
    return mPersistenceIndexKeyTelematikId.value();
}
