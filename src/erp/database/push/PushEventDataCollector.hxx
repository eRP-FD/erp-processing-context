/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/database/DatabaseModel.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/model/PrescriptionType.hxx"

#include <stdexcept>
#include <string>

namespace model
{
enum class ChannelId : uint8_t;
}

class PushEventDataCollector
{
public:
    PushEventDataCollector& setKvnr(const model::Kvnr& kvnr);
    [[nodiscard]] std::optional<model::Kvnr> kvnr() const;

    PushEventDataCollector& setHashedKvnr(const db_model::HashedKvnr& kvnr);
    [[nodiscard]] std::optional<db_model::HashedKvnr> hashedKvnr() const;

    PushEventDataCollector& setPrescriptionId(const model::PrescriptionId& prescriptionId);
    [[nodiscard]] std::optional<model::PrescriptionId> prescriptionId() const;

    PushEventDataCollector& setPrescriptions(const std::vector<model::PrescriptionId>& prescriptions);
    [[nodiscard]] std::vector<model::PrescriptionId> prescriptions() const;

    PushEventDataCollector& setChannelId(model::ChannelId channelId);
    [[nodiscard]] std::optional<model::ChannelId> channelId() const;

    PushEventDataCollector& setNotificationIdentifier(const std::string& notificationIdentifier);
    [[nodiscard]] std::optional<std::string> notificationIdentifier() const;

    PushEventDataCollector& setRequestId(const std::string& requestId);
    [[nodiscard]] std::optional<std::string> requestId() const;

    bool isReadyForPushEvent() const;

    void validate();

private:
    std::optional<model::Kvnr> mKvnr;
    std::optional<db_model::HashedKvnr> mHashedKvnr;
    std::vector<model::PrescriptionId> mPrescriptions;
    std::optional<model::ChannelId> mChannelId;
    std::optional<std::string> mNotificationIdentifier;
    std::optional<std::string> mRequestId;
    bool mEnablePushEvent{false};
};
