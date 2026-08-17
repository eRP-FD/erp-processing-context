/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "exporter/model/push/EncryptedNotification.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "exporter/VauAutTokenSigner.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/model/NumberAsStringParserWriter.hxx"
#include "library/util/Uuid.hxx"
#include "shared/util/Uuid.hxx"

#include <utility>

namespace model
{

EncryptedNotification::EncryptedNotification(std::string ciphertext, std::string timeMessageEncrypted,
                                             std::string keyIdentifier, std::string identifier,
                                             PushNotificationDevice device)
    : mCiphertext(std::move(ciphertext))
    , mTimeMessageEncrypted(std::move(timeMessageEncrypted))
    , mKeyIdentifier(std::move(keyIdentifier))
    , mIdentifier(std::move(identifier))
    , mDevice(std::move(device))
{
}

void EncryptedNotification::setPrio(PushNotificationPrio prio)
{
    mPrio = prio;
}

void EncryptedNotification::toJson(rapidjson::Value& parent, rapidjson::Document::AllocatorType& alloc) const
{
    // GEMREQ-start A_27161
    rapidjson::Value notification(rapidjson::kObjectType);
    notification.AddMember("ciphertext", rapidjson::StringRef(mCiphertext), alloc);
    // GEMREQ-end A_27161
    A_27162.start("Einbetten des Zeitstempels");
    notification.AddMember("time_message_encrypted", rapidjson::StringRef(mTimeMessageEncrypted), alloc);
    A_27162.finish();
    notification.AddMember("key_identifier", rapidjson::StringRef(mKeyIdentifier), alloc);
    if (! mIdentifier.empty())
    {
        notification.AddMember("identifier", rapidjson::StringRef(mIdentifier), alloc);
    }
    const auto prio = magic_enum::enum_name(mPrio);
    notification.AddMember("prio", rapidjson::StringRef(prio.data(), prio.size()), alloc);
    mDevice.toJson(notification, alloc);
    parent.AddMember("notification", notification.Move(), alloc);
}

std::string
serializePushNotifications(const std::vector<std::pair<std::string, EncryptedNotification>>& pushNotifications)
{
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Value array(rapidjson::kArrayType);
    for (const auto& [id, pushNotification] : pushNotifications)
    {
        rapidjson::Value entry(rapidjson::kObjectType);
        entry.AddMember("id", id, doc.GetAllocator());
        pushNotification.toJson(entry, doc.GetAllocator());
        array.PushBack(entry.Move(), doc.GetAllocator());
    }
    doc.AddMember("notifications", array.Move(), doc.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

}// model