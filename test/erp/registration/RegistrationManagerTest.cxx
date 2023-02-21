/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/database/RedisClient.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/registration/RegistrationManager.hxx"
#include "erp/util/health/ApplicationHealth.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/util/TestConfiguration.hxx"

#include <gtest/gtest.h>
#include <string>
#include <thread>// for std::this_thread::sleep_for

class RegistrationManagerTest : public testing::Test
{
public:
    const std::string teeHost = "tee-host";
    std::string getKeyName(uint16_t teePort) const
    {
        return "ERP_VAU_INSTANCE:" + teeHost + ":" + std::to_string(teePort);
    }
    const char* eventFieldName = "event";
    const char* timestampFieldName = "timestamp";
};

TEST_F(RegistrationManagerTest, Registration)//NOLINT(readability-function-cognitive-complexity)
{
    auto uniqueRedisClient =
        TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_REDIS_MOCK, true)
            ? std::unique_ptr<RedisInterface>(new MockRedisStore())
            : std::unique_ptr<RedisInterface>(new RedisClient());
    auto& redisClient = *uniqueRedisClient;
    const auto& config = Configuration::instance();
    const auto teeport = config.serverPort();
    const auto keyName = getKeyName(teeport);
    RegistrationManager registrationManager(teeHost, teeport, std::move(uniqueRedisClient));

    const model::Timestamp now = model::Timestamp::now();

    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_NO_THROW(registrationManager.registration());
    ASSERT_TRUE(registrationManager.registered());
    ASSERT_TRUE(redisClient.exists(keyName));
    ASSERT_TRUE(redisClient.hasKeyWithField(keyName, eventFieldName));
    EXPECT_EQ(redisClient.fieldValueForKey(keyName, eventFieldName).value(), "startup");
    ASSERT_TRUE(redisClient.hasKeyWithField(keyName, timestampFieldName));
    std::string startupTimeStr;
    ASSERT_NO_THROW(startupTimeStr = redisClient.fieldValueForKey(keyName, timestampFieldName).value());
    model::Timestamp startupTime = model::Timestamp::now();
    ASSERT_NO_THROW(startupTime = model::Timestamp((int64_t) std::stol(startupTimeStr)));
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ASSERT_NO_THROW(registrationManager.heartbeat());
    ASSERT_TRUE(registrationManager.registered());
    ASSERT_TRUE(redisClient.exists(keyName));
    ASSERT_TRUE(redisClient.hasKeyWithField(keyName, eventFieldName));
    EXPECT_EQ(redisClient.fieldValueForKey(keyName, eventFieldName).value(), "heartbeat");
    ASSERT_TRUE(redisClient.hasKeyWithField(keyName, timestampFieldName));
    std::string heartbeatTimeStr;
    ASSERT_NO_THROW(heartbeatTimeStr = redisClient.fieldValueForKey(keyName, timestampFieldName).value());
    model::Timestamp heartbeatTime = model::Timestamp::now();
    ASSERT_NO_THROW(heartbeatTime = model::Timestamp((int64_t) std::stol(heartbeatTimeStr)));
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ASSERT_NO_THROW(registrationManager.deregistration());
    ASSERT_FALSE(registrationManager.registered());
    ASSERT_TRUE(redisClient.exists(keyName));
    ASSERT_TRUE(redisClient.hasKeyWithField(keyName, eventFieldName));
    EXPECT_EQ(redisClient.fieldValueForKey(keyName, eventFieldName).value(), "unregister");
    ASSERT_TRUE(redisClient.hasKeyWithField(keyName, timestampFieldName));
    std::string deregisterTimeStr;
    ASSERT_NO_THROW(deregisterTimeStr = redisClient.fieldValueForKey(keyName, timestampFieldName).value());
    model::Timestamp deregisterTime = model::Timestamp::now();
    ASSERT_NO_THROW(deregisterTime = model::Timestamp((int64_t) std::stol(deregisterTimeStr)));

    EXPECT_LT(now, startupTime);
    EXPECT_LT(startupTime, heartbeatTime);
    EXPECT_LT(heartbeatTime, deregisterTime);
}

TEST_F(RegistrationManagerTest, RegistrationBasedOnApplicationHealth)//NOLINT(readability-function-cognitive-complexity)
{
    auto uniqueRedisClient =
        TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_REDIS_MOCK, true)
            ? std::unique_ptr<RedisInterface>(new MockRedisStore())
            : std::unique_ptr<RedisInterface>(new RedisClient());
    const auto& config = Configuration::instance();
    RegistrationManager registrationManager(teeHost, config.serverPort(), std::move(uniqueRedisClient));

    ApplicationHealth applicationHealth;
    ASSERT_FALSE(registrationManager.registered());
    ASSERT_NO_THROW(registrationManager.updateRegistrationBasedOnApplicationHealth(applicationHealth));
    ASSERT_FALSE(registrationManager.registered());

    for (const auto& item : magic_enum::enum_values<ApplicationHealth::Service>())
    {
        //
        applicationHealth.up(item);
    }

    ASSERT_NO_THROW(registrationManager.updateRegistrationBasedOnApplicationHealth(applicationHealth));
    ASSERT_TRUE(registrationManager.registered());

    applicationHealth.down(ApplicationHealth::Service::Bna, {});

    ASSERT_NO_THROW(registrationManager.updateRegistrationBasedOnApplicationHealth(applicationHealth));
    ASSERT_FALSE(registrationManager.registered());
}
