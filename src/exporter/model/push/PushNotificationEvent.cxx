/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/push/PushNotificationEvent.hxx"

#include <utility>

namespace model
{

PushNotificationEvent::PushNotificationEvent(int64_t id, std::string channelId, std::string identifier,
                                             std::string identifierType, db_model::HashedKvnr hashedKvnr,
                                             int retryCount, Timestamp created, std::string auditOrCommunicationIdentifier)
    : mId(id)
    , mPushNotification(std::move(channelId), std::move(identifier), std::move(identifierType))
    , mHashedKvnr(std::move(hashedKvnr))
    , mRetryCount(retryCount)
    , mCreated(created)
    , mAuditOrCommunicationIdentifier(std::move(auditOrCommunicationIdentifier))
{
}
const PushNotification& PushNotificationEvent::pushNotification() const
{
    return mPushNotification;
}
const db_model::HashedKvnr& PushNotificationEvent::hashedKvnr() const
{
    return mHashedKvnr;
}
int PushNotificationEvent::retryCount() const
{
    return mRetryCount;
}
int64_t PushNotificationEvent::id() const
{
    return mId;
}
Timestamp PushNotificationEvent::created() const
{
    return mCreated;
}
const std::string& PushNotificationEvent::auditOrCommunicationIdentifier() const
{
    return mAuditOrCommunicationIdentifier;
}

}// model