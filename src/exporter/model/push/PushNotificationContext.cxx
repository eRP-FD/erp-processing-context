/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "exporter/model/push/PushNotificationContext.hxx"

#include <utility>

namespace model
{
PushNotificationContext::PushNotificationContext(PushNotification pushNotification, PushNotificationDevice device,
                                                 EncryptionKey encryptionKey, std::string keyIdentifier,
                                                 const Timestamp& created, db_model::Blob salt, BlobId blobId,
                                                 int64_t eventId, HashedKvnr hashedKvnr, int retryCount,
                                                 std::string auditOrCommunicationIdentifier)
    : mPushNotification(std::move(pushNotification))
    , mDevice(std::move(device))
    , mEncryptionKey(std::move(encryptionKey))
    , mKeyIdentifier(std::move(keyIdentifier))
    , mAuditOrCommunicationIdentifier(std::move(auditOrCommunicationIdentifier))
    , mCreated(created)
    , mSalt(std::move(salt))
    , mBlobId(blobId)
    , mEventId(eventId)
    , mHashedKvnr(std::move(hashedKvnr))
    , mRetryCount(retryCount)
{
}

const PushNotification& PushNotificationContext::pushNotification() const
{
    return mPushNotification;
}
const PushNotificationDevice& PushNotificationContext::device() const
{
    return mDevice;
}
const EncryptionKey& PushNotificationContext::encryptionKey() const
{
    return mEncryptionKey;
}
const std::string& PushNotificationContext::keyIdentifier() const
{
    return mKeyIdentifier;
}
const Timestamp& PushNotificationContext::created() const
{
    return mCreated;
}
const std::string& PushNotificationContext::auditOrCommunicationIdentifier() const
{
    return mAuditOrCommunicationIdentifier;
}
const Uuid& PushNotificationContext::xRequestId() const
{
    return mXRequestId;
}
const db_model::Blob& PushNotificationContext::salt() const
{
    return mSalt;
}
const BlobId& PushNotificationContext::blobId() const
{
    return mBlobId;
}
int64_t PushNotificationContext::eventId() const
{
    return mEventId;
}
const HashedKvnr& PushNotificationContext::hashedKvnr() const
{
    return mHashedKvnr;
}
int PushNotificationContext::retryCount() const
{
    return mRetryCount;
}
std::chrono::seconds PushNotificationContext::rescheduleDelay() const
{
    return std::chrono::minutes{1U << (mRetryCount + 1)};
}

void PushNotificationContext::setEncryptionKey(EncryptionKey&& encryptionKey)
{
    mEncryptionKey = std::move(encryptionKey);
}

}
