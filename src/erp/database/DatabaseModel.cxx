/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
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
                     const fhirtools::Timestamp& initAuthoredOn,
                     const fhirtools::Timestamp& initLastModified)
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
    , recorded(fhirtools::Timestamp::now())
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

    if (chargeInformation.chargeItem.markingFlag().has_value())
    {
        markingFlag = codec.encode(chargeInformation.chargeItem.markingFlag().value().serializeToJsonString(), key,
                                   Compression::DictionaryUse::Default_json);
    }

    ModelExpect(chargeInformation.chargeItem.subjectKvnr(), "Missing kvnr");
    kvnr =
        codec.encode(chargeInformation.chargeItem.subjectKvnr().value(), key, Compression::DictionaryUse::Default_json);

    if (chargeInformation.prescription && chargeInformation.unsignedPrescription)
    {
        prescription =
            codec.encode(chargeInformation.prescription->data().value(), key, Compression::DictionaryUse::Default_xml);
        prescriptionJson = codec.encode(chargeInformation.unsignedPrescription.value().serializeToJsonString(), key,
                                        ::Compression::DictionaryUse::Default_json);
    }

    ModelExpect(chargeInformation.dispenseItem.data(), "Missing signed dispense data.");
    billingData =
        codec.encode(chargeInformation.dispenseItem.data().value(), key, ::Compression::DictionaryUse::Default_xml);

    billingDataJson = codec.encode(chargeInformation.unsignedDispenseItem.serializeToJsonString(), key,
                                   ::Compression::DictionaryUse::Default_json);

    if (chargeInformation.receipt)
    {
        receiptXml = codec.encode(chargeInformation.receipt.value().serializeToXmlString(), key,
                                  ::Compression::DictionaryUse::Default_xml);
        receiptJson = codec.encode(chargeInformation.receipt.value().serializeToJsonString(), key,
                                   ::Compression::DictionaryUse::Default_json);
    }
}

::model::ChargeInformation ChargeItem::toChargeInformation(const ::SafeString& key, const ::DataBaseCodec& codec) const
{
    try
    {
        auto prescriptionBundle = ::model::Bundle::fromJsonNoValidation(codec.decode(prescriptionJson.value(), key));
        auto billingBundle = ::model::Bundle::fromJsonNoValidation(codec.decode(billingDataJson, key));

        auto chargeInformation = ::model::ChargeInformation{
            ::model::ChargeItem{},
            ::model::Binary{prescriptionBundle.getId().toString(), codec.decode(prescription.value(), key)},
            ::std::move(prescriptionBundle),
            ::model::Binary{billingBundle.getId().toString(), codec.decode(billingData, key)},
            ::std::move(billingBundle),
            ::model::Bundle::fromJsonNoValidation(codec.decode(receiptJson.value(), key))};

        chargeInformation.chargeItem.setId(prescriptionId);
        chargeInformation.chargeItem.setPrescriptionId(prescriptionId);
        chargeInformation.chargeItem.setSubjectKvnr(codec.decode(kvnr, key));
        chargeInformation.chargeItem.setEntererTelematikId(codec.decode(enterer, key));
        chargeInformation.chargeItem.setEnteredDate(enteredDate);
        chargeInformation.chargeItem.setAccessCode(codec.decode(accessCode, key));

        if (markingFlag.has_value())
        {
            chargeInformation.chargeItem.setMarkingFlag(
                ::model::ChargeItemMarkingFlag::fromJsonNoValidation(codec.decode(markingFlag.value(), key)));
        }

        A_22134.start("KBV prescription bundle");
        chargeInformation.chargeItem.setSupportingInfoReference(
            ::model::ChargeItem::SupportingInfoType::prescriptionItem, chargeInformation.unsignedPrescription.value());
        A_22134.finish();


        A_22137.start("Set PKV dispense item reference in ChargeItem");
        chargeInformation.chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItem,
                                                                chargeInformation.unsignedDispenseItem);
        A_22137.finish();

        A_22135.start("Receipt");
        chargeInformation.chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::receipt,
                                                                chargeInformation.receipt.value());
        A_22135.finish();

        return chargeInformation;
    }
    catch (const ::std::exception& exception)
    {
        ModelFail("Unable to reconstruct charge item from database:"s + exception.what());
    }
}
