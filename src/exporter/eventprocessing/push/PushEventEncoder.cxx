/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/eventprocessing/push/PushEventEncoder.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "exporter/model/push/EncryptedNotification.hxx"
#include "exporter/model/push/PushNotificationContext.hxx"
#include "exporter/model/push/PushNotificationDevice.hxx"
#include "shared/model/Timestamp.hxx"

#include <date/date.h>
#include <fmt/format.h>

model::EncryptedNotification PushEventEncoder::encode(const model::PushNotificationContext& pushNotificationContext)
{
    // GEMREQ-start A_27161
    Expect(! encryptionKeyNeedsUpdate(pushNotificationContext.encryptionKey()), "EncryptionKey is outdated");
    auto ciphertext = pushNotificationContext.encryptionKey().encryptMessage(
        pushNotificationContext.pushNotification().serializeToJsonString());
    A_27162.start("Einbetten des Zeitstempels");
    model::EncryptedNotification encryptedNotification(
        ciphertext, pushNotificationContext.encryptionKey().monthInfoString(), pushNotificationContext.keyIdentifier(),
        pushNotificationContext.auditOrCommunicationIdentifier(), pushNotificationContext.device());
    // GEMREQ-end A_27161
    return encryptedNotification;
}

bool PushEventEncoder::encryptionKeyNeedsUpdate(const model::EncryptionKey& encryptionKey)
{
    return ! encryptionKey.isUpToDate();
}

model::EncryptionKey PushEventEncoder::updateEncryptionKey(const model::EncryptionKey& encryptionKey)
{
    return model::EncryptionKey::deriveFromPreviousMonthlyKey(encryptionKey);
}
