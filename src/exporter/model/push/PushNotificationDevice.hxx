/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "shared/database/DatabaseModel.hxx"
#include "shared/model/push/Pusher.hxx"

#include <rapidjson/document.h>
#include <string>

namespace model
{

// https://gematik.github.io/gem-push-notifications-concept/1.2/index.html#push_gateway_openapi.html
class PushNotificationDevice
{
public:
    PushNotificationDevice(std::string appId, std::string pushKey, std::chrono::seconds pushkeyTs, PusherData data,
                           std::string displayName);

    void toJson(rapidjson::Value& parent, rapidjson::Document::AllocatorType& alloc) const;

    [[nodiscard]] const std::string& appId() const;
    [[nodiscard]] const std::string& pushkey() const;
    [[nodiscard]] std::chrono::seconds pushkeyTs() const;
    [[nodiscard]] const PusherData& data() const;
    [[nodiscard]] const std::string& displayName() const;

private:
    std::string mAppId;
    std::string mPushkey;
    std::chrono::seconds mPushkeyTs;
    PusherData mData;
    std::string mDisplayName;
};

}// model
