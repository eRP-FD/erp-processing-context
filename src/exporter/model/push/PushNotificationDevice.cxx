/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "PushNotificationDevice.hxx"

namespace model
{

PushNotificationDevice::PushNotificationDevice(std::string appId, std::string pushKey, std::chrono::seconds pushkeyTs,
                                               PusherData data, std::string displayName)
    : mAppId(std::move(appId))
    , mPushkey(std::move(pushKey))
    , mPushkeyTs(pushkeyTs)
    , mData(std::move(data))
    , mDisplayName(std::move(displayName))
{
}

void PushNotificationDevice::toJson(rapidjson::Value& parent, rapidjson::Document::AllocatorType& alloc) const
{
    rapidjson::Value device(rapidjson::kObjectType);
    device.AddMember("app_id", rapidjson::StringRef(mAppId.c_str()), alloc);
    device.AddMember("pushkey", rapidjson::StringRef(mPushkey.c_str()), alloc);
    device.AddMember("pushkey_ts", mPushkeyTs.count(), alloc);
    if (mData.mUserData && ! mData.mUserData->ObjectEmpty())
    {
        rapidjson::Value userData(rapidjson::kObjectType);
        userData.CopyFrom(*mData.mUserData, alloc);
        device.AddMember("data", userData.Move(), alloc);
    }
    parent.AddMember("device", device.Move(), alloc);
}

const std::string& PushNotificationDevice::appId() const
{
    return mAppId;
}

const std::string& PushNotificationDevice::pushkey() const
{
    return mPushkey;
}

std::chrono::seconds PushNotificationDevice::pushkeyTs() const
{
    return mPushkeyTs;
}
const PusherData& PushNotificationDevice::data() const
{
    return mData;
}
const std::string& PushNotificationDevice::displayName() const
{
    return mDisplayName;
}

}// model