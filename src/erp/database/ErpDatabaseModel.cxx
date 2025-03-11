/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/ErpDatabaseModel.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/Jwt.hxx"
#include "shared/crypto/Pbkdf2Hmac.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Expect.hxx"

#include <rapidjson/writer.h>
#include <algorithm>

using namespace db_model;
using namespace ::std::literals;

db_model::Task::Task(const model::PrescriptionId& initPrescriptionId, BlobId initKeyBlobId, db_model::Blob initSalt,
                     model::Task::Status initStatus, model::Timestamp initLastStatusChanged,
                     const model::Timestamp& initAuthoredOn, const model::Timestamp& initLastModified)
    : prescriptionId(initPrescriptionId)
    , blobId(initKeyBlobId)
    , salt(std::move(initSalt))
    , status(initStatus)
    , lastStatusChange(initLastStatusChanged)
    , authoredOn(initAuthoredOn)
    , lastModified(initLastModified)
{
}

MedicationDispense::MedicationDispense(model::PrescriptionId initPrescriptionId,
                                       db_model::EncryptedBlob initMedicationDispense, BlobId initKeyBlobId,
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
    ModelExpect(chargeInformation.dispenseItem.has_value() && chargeInformation.dispenseItem->data(),
                "Missing signed dispense data.");
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
            ref.prescription =
                model::Binary{prescriptionBundle.getId().toString(), codec.decode(prescription.value(), key)};
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
            const std::string str{reinterpret_cast<const char*>(markingFlags.value().data()),
                                  markingFlags.value().size()};
            ref.chargeItem.setMarkingFlags(::model::ChargeItemMarkingFlags::fromJsonNoValidation(str));
        }

        A_22134.start("KBV prescription bundle");
        ref.chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItemBundle);
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

AccessTokenIdentity::AccessTokenIdentity(const JWT& jwt)
    : mId(jwt.stringForClaim(JWT::idNumberClaim).value_or(""))
    , mName(jwt.stringForClaim(JWT::organizationNameClaim).value_or("unbekannt"))
    , mOid(jwt.stringForClaim(JWT::professionOIDClaim).value_or(""))
{
    ModelExpect(! mId.id().empty() && ! mOid.empty(), "incomplete JWT, idNumber or professionOID missing");
}

AccessTokenIdentity::AccessTokenIdentity(const model::TelematikId& id, std::string_view name, std::string_view oid)
    : mId(id)
    , mName(name)
    , mOid(oid)
{
}

std::string AccessTokenIdentity::getJson() const
{
    rapidjson::Document d;
    d.SetObject();
    d.AddMember("id", mId.id(), d.GetAllocator());
    d.AddMember("name", mName, d.GetAllocator());
    d.AddMember("oid", mOid, d.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    d.Accept(writer);

    return buffer.GetString();
}

const model::TelematikId& AccessTokenIdentity::getId() const
{
    return mId;
}

const std::string& AccessTokenIdentity::getName() const
{
    return mName;
}

const std::string& AccessTokenIdentity::getOid() const
{
    return mOid;
}

AccessTokenIdentity AccessTokenIdentity::fromJson(const std::string& json)
{
    rapidjson::Document doc;
    doc.Parse(json);
    if (doc.HasParseError())
    {
        Fail("invalid custom json representation of JWT");
    }
    return AccessTokenIdentity(model::TelematikId(std::string(doc["id"].GetString())),
                               std::string(doc["name"].GetString()), std::string(doc["oid"].GetString()));
}
