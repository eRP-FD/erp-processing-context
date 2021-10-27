/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/mock/MockRedisStore.hxx"
#include "test/util/TestConfiguration.hxx"

#include <gtest/gtest.h>
#include <string>
#include <thread>// for std::this_thread::sleep_for

#include "erp/database/RedisClient.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/registration/RegistrationManager.hxx"


TEST(RegistrationManagerTest, Registration)
{
    auto uniqueRedisClient = TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_REDIS_MOCK, true) ?
        std::unique_ptr<RedisInterface>(new MockRedisStore()) : std::unique_ptr<RedisInterface>(new RedisClient());
    auto& redisClient = *uniqueRedisClient;

    const std::string teeHost = "tee-host";
    const uint16_t teePort = 9090;

    RegistrationManager registrationManager(teeHost, teePort, std::move(uniqueRedisClient));

    const auto keyName = "ERP_VAU_INSTANCE:" + teeHost + ":" + std::to_string(teePort);
    const auto* eventFieldName = "event";
    const auto* timestampFieldName = "timestamp";

    const model::Timestamp now = model::Timestamp::now();

    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_NO_THROW(registrationManager.registration());
    ASSERT_TRUE(redisClient.exists(keyName));
    ASSERT_TRUE(redisClient.hasKeyWithField(keyName, eventFieldName));
    EXPECT_EQ(redisClient.fieldValueForKey(keyName, eventFieldName).value(), "startup");
    ASSERT_TRUE(redisClient.hasKeyWithField(keyName, timestampFieldName));
    std::string startupTimeStr;
    ASSERT_NO_THROW(startupTimeStr = redisClient.fieldValueForKey(keyName, timestampFieldName).value());
    model::Timestamp startupTime = model::Timestamp::now();
    ASSERT_NO_THROW(startupTime = model::Timestamp((int64_t)std::stol(startupTimeStr)));
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ASSERT_NO_THROW(registrationManager.heartbeat());
    ASSERT_TRUE(redisClient.exists(keyName));
    ASSERT_TRUE(redisClient.hasKeyWithField(keyName, eventFieldName));
    EXPECT_EQ(redisClient.fieldValueForKey(keyName, eventFieldName).value(), "heartbeat");
    ASSERT_TRUE(redisClient.hasKeyWithField(keyName, timestampFieldName));
    std::string heartbeatTimeStr;
    ASSERT_NO_THROW(heartbeatTimeStr = redisClient.fieldValueForKey(keyName, timestampFieldName).value());
    model::Timestamp heartbeatTime = model::Timestamp::now();
    ASSERT_NO_THROW(heartbeatTime = model::Timestamp((int64_t)std::stol(heartbeatTimeStr)));
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ASSERT_NO_THROW(registrationManager.deregistration());
    ASSERT_TRUE(redisClient.exists(keyName));
    ASSERT_TRUE(redisClient.hasKeyWithField(keyName, eventFieldName));
    EXPECT_EQ(redisClient.fieldValueForKey(keyName, eventFieldName).value(), "unregister");
    ASSERT_TRUE(redisClient.hasKeyWithField(keyName, timestampFieldName));
    std::string deregisterTimeStr;
    ASSERT_NO_THROW(deregisterTimeStr = redisClient.fieldValueForKey(keyName, timestampFieldName).value());
    model::Timestamp deregisterTime = model::Timestamp::now();
    ASSERT_NO_THROW(deregisterTime = model::Timestamp((int64_t)std::stol(deregisterTimeStr)));

    EXPECT_LT(now, startupTime);
    EXPECT_LT(startupTime, heartbeatTime);
    EXPECT_LT(heartbeatTime, deregisterTime);
}