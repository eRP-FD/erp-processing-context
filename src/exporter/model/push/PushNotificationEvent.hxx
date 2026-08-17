/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "PushNotification.hxx"
#include "shared/database/DatabaseModel.hxx"

namespace model
{

class PushNotificationEvent
{
public:
    PushNotificationEvent(int64_t id, std::string channelId, std::string identifier, std::string identifierType,
                          db_model::HashedKvnr hashedKvnr, int retryCount, Timestamp created, std::string auditOrCommunicationIdentifier);

    [[nodiscard]] const PushNotification& pushNotification() const;
    [[nodiscard]] const db_model::HashedKvnr& hashedKvnr() const;
    [[nodiscard]] int retryCount() const;
    [[nodiscard]] int64_t id() const;
    [[nodiscard]] model::Timestamp created() const;
    [[nodiscard]] const std::string& auditOrCommunicationIdentifier() const;

private:
    int64_t mId;
    PushNotification mPushNotification;
    db_model::HashedKvnr mHashedKvnr;
    int mRetryCount;
    Timestamp mCreated;
    std::string mAuditOrCommunicationIdentifier;
};

}// model
