/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/push/EncryptedNotification.hxx"

#include <gtest/gtest.h>

class EncryptedNotificationTest : public testing::Test
{
};

TEST_F(EncryptedNotificationTest, serializeEmpty)
{
    auto json = model::serializePushNotifications({});
    EXPECT_EQ(json, R"({"notifications":[]})");
}

TEST_F(EncryptedNotificationTest, serializeOne)
{
    const model::EncryptedNotification encryptedNotification{
        "ciphertext", "yyyy-mm", "key-identifier", "identifier",
        model::PushNotificationDevice{"appt-id", "push-key", std::chrono::seconds{1},
                                      model::PusherData{"https://x/push/v1/", std::nullopt}, "displayName"}};
    auto json = model::serializePushNotifications({std::make_pair("ID", encryptedNotification)});
    EXPECT_EQ(
        json,
        R"({"notifications":[{"id":"ID","notification":{"ciphertext":"ciphertext","time_message_encrypted":"yyyy-mm","key_identifier":"key-identifier","identifier":"identifier","prio":"low","device":{"app_id":"appt-id","pushkey":"push-key","pushkey_ts":1}}}]})")
        << json;
}

TEST_F(EncryptedNotificationTest, serializeTwo)
{
    const model::EncryptedNotification encryptedNotification{
        "ciphertext", "yyyy-mm", "key-identifier", "identifier",
        model::PushNotificationDevice{"appt-id", "push-key", std::chrono::seconds{1},
                                      model::PusherData{"https://x/push/v1/", std::nullopt}, "displayName"}};
    model::EncryptedNotification encryptedNotification2{
        "ciphertext2", "yyyy-mm", "key-identifier-2", "identifier-2",
        model::PushNotificationDevice{"appt-id-2", "push-key-2", std::chrono::seconds{1},
                                      model::PusherData{"https://x/push/v1/", std::nullopt}, "displayName"}};
    encryptedNotification2.setPrio(model::PushNotificationPrio::high);
    auto json = model::serializePushNotifications(
        {std::make_pair("ID", encryptedNotification), std::make_pair("ID2", encryptedNotification2)});
    EXPECT_EQ(
        json,
        R"({"notifications":[{"id":"ID","notification":{"ciphertext":"ciphertext","time_message_encrypted":"yyyy-mm","key_identifier":"key-identifier","identifier":"identifier","prio":"low","device":{"app_id":"appt-id","pushkey":"push-key","pushkey_ts":1}}},{"id":"ID2","notification":{"ciphertext":"ciphertext2","time_message_encrypted":"yyyy-mm","key_identifier":"key-identifier-2","identifier":"identifier-2","prio":"high","device":{"app_id":"appt-id-2","pushkey":"push-key-2","pushkey_ts":1}}}]})")
        << json;
}
