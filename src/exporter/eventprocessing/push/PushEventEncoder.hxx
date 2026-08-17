/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include <string>


namespace model
{
class PushNotificationContext;
class EncryptionKey;
class EncryptedNotification;
}

class PushEventEncoder
{
public:
    static model::EncryptedNotification encode(const model::PushNotificationContext& pushNotificationContext);
    static bool encryptionKeyNeedsUpdate(const model::EncryptionKey& encryptionKey);
    static model::EncryptionKey updateEncryptionKey(const model::EncryptionKey& encryptionKey);
};
