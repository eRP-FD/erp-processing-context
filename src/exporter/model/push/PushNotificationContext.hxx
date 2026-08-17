/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "PushNotification.hxx"
#include "PushNotificationDevice.hxx"
#include "library/util/Uuid.hxx"
#include "shared/model/push/EncryptionKey.hxx"
#include "shared/util/Uuid.hxx"

#include <string>

namespace model
{

class PushNotificationContext
{
public:
    PushNotificationContext(PushNotification pushNotification, PushNotificationDevice device,
                            EncryptionKey encryptionKey, std::string keyIdentifier, const Timestamp& created,
                            db_model::Blob salt, BlobId blobId, int64_t eventId, HashedKvnr hashedKvnr, int mRetryCount,
                            std::string auditOrCommunicationIdentifier);

    [[nodiscard]] const PushNotification& pushNotification() const;
    [[nodiscard]] const PushNotificationDevice& device() const;
    [[nodiscard]] const EncryptionKey& encryptionKey() const;
    [[nodiscard]] const std::string& keyIdentifier() const;
    [[nodiscard]] const Timestamp& created() const;
    [[nodiscard]] const std::string& auditOrCommunicationIdentifier() const;
    [[nodiscard]] const Uuid& xRequestId() const;
    [[nodiscard]] const db_model::Blob& salt() const;
    [[nodiscard]] const BlobId& blobId() const;
    [[nodiscard]] int64_t eventId() const;
    [[nodiscard]] const HashedKvnr& hashedKvnr() const;
    [[nodiscard]] int retryCount() const;
    [[nodiscard]] std::chrono::seconds rescheduleDelay() const;

    void setEncryptionKey(EncryptionKey&& encryptionKey);

private:
    PushNotification mPushNotification;
    PushNotificationDevice mDevice;
    EncryptionKey mEncryptionKey;
    std::string mKeyIdentifier;
    std::string mAuditOrCommunicationIdentifier;
    Uuid mXRequestId;
    Timestamp mCreated;
    // for key update:
    db_model::Blob mSalt;
    BlobId mBlobId;
    // for delete
    int64_t mEventId;
    HashedKvnr mHashedKvnr;
    // for re-schedule
    int mRetryCount;
};

}
