/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "erp/database/push/PushEventDataCollector.hxx"
#include "erp/model/push/Channels.hxx"

PushEventDataCollector& PushEventDataCollector::setKvnr(const model::Kvnr& kvnr)
{
    mKvnr.emplace(kvnr);
    return *this;
}

std::optional<model::Kvnr> PushEventDataCollector::kvnr() const
{
    return mKvnr;
}

PushEventDataCollector& PushEventDataCollector::setHashedKvnr(const db_model::HashedKvnr& hashedKvnr)
{
    mHashedKvnr.emplace(hashedKvnr);
    return *this;
}

std::optional<db_model::HashedKvnr> PushEventDataCollector::hashedKvnr() const
{
    return mHashedKvnr;
}

PushEventDataCollector& PushEventDataCollector::setPrescriptionId(const model::PrescriptionId& prescriptionId)
{
    mPrescriptions.clear();
    mPrescriptions.emplace_back(prescriptionId);
    return *this;
}

std::optional<model::PrescriptionId> PushEventDataCollector::prescriptionId() const
{
    if (!mPrescriptions.empty())
    {
        return mPrescriptions.front();
    }
    return std::nullopt;
}

PushEventDataCollector& PushEventDataCollector::setPrescriptions(const std::vector<model::PrescriptionId>& prescriptions)
{
    mPrescriptions.clear();
    mPrescriptions.reserve(prescriptions.size());
    std::ranges::copy(prescriptions, std::back_inserter(mPrescriptions));
    return *this;
}

[[nodiscard]] std::vector<model::PrescriptionId> PushEventDataCollector::prescriptions() const
{
    return mPrescriptions;
}

PushEventDataCollector& PushEventDataCollector::setChannelId(model::ChannelId channelId)
{
    mChannelId.emplace(channelId);
    return *this;
}

[[nodiscard]] std::optional<model::ChannelId> PushEventDataCollector::channelId() const
{
    return mChannelId;
}

PushEventDataCollector& PushEventDataCollector::setNotificationIdentifier(const std::string& notificationIdentifier)
{
    mNotificationIdentifier.emplace(notificationIdentifier);
    return *this;
}

std::optional<std::string> PushEventDataCollector::notificationIdentifier() const
{
    return mNotificationIdentifier;
}

PushEventDataCollector& PushEventDataCollector::setRequestId(const std::string& requestId)
{
    mRequestId.emplace(requestId);
    return *this;
}

[[nodiscard]] std::optional<std::string> PushEventDataCollector::requestId() const
{
    return mRequestId;
}

bool PushEventDataCollector::isReadyForPushEvent() const
{
    return mEnablePushEvent;
}

void PushEventDataCollector::validate()
{
    mEnablePushEvent = ! mPrescriptions.empty() && mNotificationIdentifier && mHashedKvnr && mChannelId &&
                       mChannelId != model::ChannelId::unused;
}
