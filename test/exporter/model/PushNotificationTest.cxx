/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/push/PushNotification.hxx"

#include <gtest/gtest.h>

class PushNotificationTest : public testing::Test
{
};

TEST_F(PushNotificationTest, serializeJson)
{
    const model::PushNotification pushNotification{"channel-id", "identifier", "identifierTyp"};
    auto json = pushNotification.serializeToJsonString();
    EXPECT_EQ(json, R"({"ChannelId":"channel-id","Identifier":"identifier","IdentifierType":"identifierTyp"})") << json;
}
