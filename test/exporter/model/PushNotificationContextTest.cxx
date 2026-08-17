/*
* (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/push/PushNotificationContext.hxx"

#include <gtest/gtest.h>

class PushNotificationContextTest : public testing::Test
{
public:
    model::PushNotificationContext makePushNotificationContext(int retryCount)
    {
        return model::PushNotificationContext{
            model::PushNotification{"channeldId", "identifier", "identifierType"},
            model::PushNotificationDevice{"appId", "pushKey", std::chrono::seconds{0}, model::PusherData{}, "displayName"},
            model::EncryptionKey{SafeString("shared-secret1111111111111111111"),
                                 SafeString("message-key111111111111111111111"), fmt::format("{}-{}", 2026, 7)},
            "",
            model::Timestamp::now(),
            {},
            {},
            0,
            {""},
            retryCount,
            "auditId"};
    }
};

TEST_F(PushNotificationContextTest, rescheduleDelay)
{
    EXPECT_EQ(makePushNotificationContext(0).rescheduleDelay(), std::chrono::seconds{120});
    EXPECT_EQ(makePushNotificationContext(1).rescheduleDelay(), std::chrono::seconds{240});
    EXPECT_EQ(makePushNotificationContext(2).rescheduleDelay(), std::chrono::seconds{480});
    EXPECT_EQ(makePushNotificationContext(3).rescheduleDelay(), std::chrono::seconds{960});
    EXPECT_EQ(makePushNotificationContext(4).rescheduleDelay(), std::chrono::seconds{1920});
    EXPECT_EQ(makePushNotificationContext(5).rescheduleDelay(), std::chrono::seconds{3840});
    EXPECT_EQ(makePushNotificationContext(6).rescheduleDelay(), std::chrono::seconds{7680});
    EXPECT_EQ(makePushNotificationContext(7).rescheduleDelay(), std::chrono::seconds{15360});
    EXPECT_EQ(makePushNotificationContext(8).rescheduleDelay(), std::chrono::seconds{30720}); //~8,5h
    EXPECT_EQ(makePushNotificationContext(9).rescheduleDelay(), std::chrono::seconds{61440}); //~17h
    EXPECT_EQ(makePushNotificationContext(10).rescheduleDelay(), std::chrono::seconds{122880}); //~34h
}
