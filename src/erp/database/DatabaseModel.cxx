/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/DatabaseModel.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/crypto/Pbkdf2Hmac.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/Expect.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <algorithm>

using namespace db_model;
using namespace ::std::literals;

postgres_bytea_view db_model::Blob::binarystring() const
{
    return {data(), size()};
}

std::string db_model::Blob::toHex() const
{
    gsl::span<const char> span(reinterpret_cast<const char*>(data()), size());
    return ByteHelper::toHex(span);
}

db_model::Blob::Blob(std::vector<std::byte> && blob)
    : vector<std::byte>(std::move(blob))
{
}

db_model::Blob::Blob(const postgres_bytea_view& pqxxBin)
    : vector<std::byte>(pqxxBin.begin(), pqxxBin.end())
{
}

Blob::Blob(const std::vector<uint8_t>& vec)
    : vector<std::byte>(reinterpret_cast<const std::byte*>(vec.data()),
                        reinterpret_cast<const std::byte*>(vec.data()) + vec.size())
{
}

void Blob::append(const std::string_view& str)
{
    reserve(str.size() + size());
    insert(end(), reinterpret_cast<const std::byte*>(str.data()),
           reinterpret_cast<const std::byte*>(str.data()) + str.size());
}

db_model::EncryptedBlob::EncryptedBlob(std::vector<std::byte> &&encryptedBlob)
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

HashedKvnr HashedKvnr::fromKvnr(const model::Kvnr& kvnr,
                                const SafeString& persistencyIndexKey)
{
    ErpExpect(kvnr.valid(), HttpStatus::BadRequest, "Invalid KVNR");

    return HashedKvnr{
        db_model::HashedId(db_model::EncryptedBlob(Pbkdf2Hmac::deriveKey(kvnr.id(), persistencyIndexKey)))};
}

HashedTelematikId HashedTelematikId::fromTelematikId(const model::TelematikId& id, const SafeString& persistencyIndexKey)
{
    return HashedTelematikId{
        db_model::HashedId(db_model::EncryptedBlob(Pbkdf2Hmac::deriveKey(id.id(), persistencyIndexKey)))};
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
    : prescriptionId(initPrescriptionId)
    , medicationDispense(std::move(initMedicationDispense))
    , blobId(initKeyBlobId)
    , salt(std::move(initSalt))
{
}

AuditData::AuditData(model::AuditEvent::AgentType agentType, model::AuditEventId eventId,
                     std::optional<EncryptedBlob> metaData, model::AuditEvent::Action action, HashedKvnr insurantKvnr,
                     int16_t deviceId, std::optional<model::PrescriptionId> prescriptionId,
                     std::optional<BlobId> blobId)
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

::db_model::ChargeItem::ChargeItem(const ::model::PrescriptionId& id)
    : prescriptionId{id}
{
}

::db_model::ChargeItem::ChargeItem(const ::model::ChargeInformation& chargeInformation, const ::BlobId& newBlobId,
                                   const Blob& newSalt, const ::SafeString& key, const ::DataBaseCodec& codec)
    : prescriptionId{chargeInformation.chargeItem.prescriptionId().value()}
    , blobId{newBlobId}
    , salt{newSalt}
{
    ModelExpect(chargeInformation.chargeItem.entererTelematikId(), "Missing enterer telematik id.");
    enterer = codec.encode(chargeInformation.chargeItem.entererTelematikId().value(), key,
                           Compression::DictionaryUse::Default_json);

    ModelExpect(chargeInformation.chargeItem.enteredDate(), "Missing entered date.");
    enteredDate = chargeInformation.chargeItem.enteredDate().value();

    lastModified = ::model::Timestamp::now();

    ModelExpect(chargeInformation.chargeItem.accessCode(), "Missing access code.");
    accessCode =
        codec.encode(chargeInformation.chargeItem.accessCode().value(), key, Compression::DictionaryUse::Default_json);

    if (chargeInformation.chargeItem.markingFlags().has_value())
    {
        Blob blob;
        blob.append(chargeInformation.chargeItem.markingFlags().value().serializeToJsonString());
        markingFlags = blob;
    }

    ModelExpect(chargeInformation.chargeItem.subjectKvnr(), "Missing kvnr");
    kvnr = codec.encode(chargeInformation.chargeItem.subjectKvnr().value().id(), key,
                        Compression::DictionaryUse::Default_json);

    // GEMREQ-start A_22134#prescription
    if (chargeInformation.prescription && chargeInformation.unsignedPrescription)
    {
        prescription =
            codec.encode(chargeInformation.prescription->data().value(), key, Compression::DictionaryUse::Default_xml);
        prescriptionJson = codec.encode(chargeInformation.unsignedPrescription.value().serializeToJsonString(), key,
                                        ::Compression::DictionaryUse::Default_json);
    }
    // GEMREQ-end A_22134#prescription

    // GEMREQ-start A_22137#chargeItemBilling
    ModelExpect(chargeInformation.dispenseItem.has_value() && chargeInformation.dispenseItem->data(), "Missing signed dispense data.");
    billingData =
        codec.encode(chargeInformation.dispenseItem->data().value(), key, ::Compression::DictionaryUse::Default_xml);

    billingDataJson = codec.encode(chargeInformation.unsignedDispenseItem->serializeToJsonString(), key,
                                   ::Compression::DictionaryUse::Default_json);
    // GEMREQ-end A_22137#chargeItemBilling

    // GEMREQ-start A_22135-01#receipt
    if (chargeInformation.receipt)
    {
        receiptXml = codec.encode(chargeInformation.receipt.value().serializeToXmlString(), key,
                                  ::Compression::DictionaryUse::Default_xml);
        receiptJson = codec.encode(chargeInformation.receipt.value().serializeToJsonString(), key,
                                   ::Compression::DictionaryUse::Default_json);
    }
    // GEMREQ-end A_22135-01#receipt
}

::model::ChargeInformation ChargeItem::toChargeInformation(const std::optional<DatabaseCodecWithKey>& codecAndKey) const
{
    model::ChargeInformation ref = model::ChargeInformation{.chargeItem = model::ChargeItem{},
                                                            .prescription = std::nullopt,
                                                            .unsignedPrescription = std::nullopt,
                                                            .dispenseItem = std::nullopt,
                                                            .unsignedDispenseItem = std::nullopt,
                                                            .receipt = std::nullopt};
    try
    {
        if (codecAndKey.has_value())
        {
            const auto& codec = codecAndKey->codec;
            const auto& key = codecAndKey->key;
            auto prescriptionBundle =
                ::model::Bundle::fromJsonNoValidation(codec.decode(prescriptionJson.value(), key));
            auto billingBundle =
                ::model::AbgabedatenPkvBundle::fromJsonNoValidation(codec.decode(billingDataJson, key));
            auto receiptBundle = ::model::Bundle::fromJsonNoValidation(codec.decode(receiptJson.value(), key));
            ref.prescription = model::Binary{prescriptionBundle.getId().toString(), codec.decode(prescription.value(), key)};
            ref.unsignedPrescription = std::move(prescriptionBundle);
            ref.dispenseItem = model::Binary{billingBundle.getId().toString(), codec.decode(billingData, key)};
            ref.unsignedDispenseItem = std::move(billingBundle);
            ref.receipt = std::move(receiptBundle);

            ref.chargeItem.setSubjectKvnr(codec.decode(kvnr, key));
            ref.chargeItem.setEntererTelematikId(codec.decode(enterer, key));
            if (! accessCode.empty())
            {
                ref.chargeItem.setAccessCode(codec.decode(accessCode, key));
            }
        }

        ref.chargeItem.setId(prescriptionId);
        ref.chargeItem.setPrescriptionId(prescriptionId);
        ref.chargeItem.setEnteredDate(enteredDate);
        if (markingFlags.has_value())
        {
            const std::string str{ reinterpret_cast<const char*>(markingFlags.value().data()), markingFlags.value().size() };
            ref.chargeItem.setMarkingFlags(::model::ChargeItemMarkingFlags::fromJsonNoValidation(str));
        }

        A_22134.start("KBV prescription bundle");
        ref.chargeItem.setSupportingInfoReference(
            ::model::ChargeItem::SupportingInfoType::prescriptionItemBundle);
        A_22134.finish();

        A_22137.start("Set PKV dispense item reference in ChargeItem");
        ref.chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItemBundle);
        A_22137.finish();

        A_22135_01.start("Receipt");
        ref.chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::receiptBundle);
        A_22135_01.finish();

        return ref;
    }
    catch (const ::std::exception& exception)
    {
        ModelFail("Unable to reconstruct charge item from database:"s + exception.what());
    }
}
