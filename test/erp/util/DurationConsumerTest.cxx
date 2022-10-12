/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/DurationConsumer.hxx"

#include "erp/util/TLog.hxx"

#include <atomic>
#include <gtest/gtest.h>
#include <thread>


class DurationConsumerTest : public testing::Test
{
};


TEST_F(DurationConsumerTest, DurationConsumerGuard)
{
    EXPECT_FALSE(DurationConsumer::getCurrent().isInitialized());

    {
        DurationConsumerGuard guard ("test", {});

        EXPECT_TRUE(DurationConsumer::getCurrent().isInitialized());
    }

    EXPECT_FALSE(DurationConsumer::getCurrent().isInitialized());
}


TEST_F(DurationConsumerTest, DurationConsumerGuard_failForSecondSetSessionIdentifier)
{
    DurationConsumerGuard guard ("test", {});

    EXPECT_TRUE(DurationConsumer::getCurrent().isInitialized());

    EXPECT_ANY_THROW(DurationConsumer::getCurrent().initialize("test", {}));
}


TEST_F(DurationConsumerTest, DurationConsumerGuard_failForSecondResetSessionIdentifier)
{
    {
        DurationConsumerGuard guard ("test", {});
    }
    EXPECT_FALSE(DurationConsumer::getCurrent().isInitialized());

    EXPECT_ANY_THROW(DurationConsumer::getCurrent().reset());
}


TEST_F(DurationConsumerTest, DurationConsumer_threadLocalInstances)//NOLINT(readability-function-cognitive-complexity)
{
    DurationConsumerGuard guard ("thread 1", {});
    EXPECT_TRUE(DurationConsumer::getCurrent().isInitialized());

    auto thread = std::thread([]
    {
        EXPECT_FALSE(DurationConsumer::getCurrent().isInitialized());
        DurationConsumerGuard guard ("thread 2", {});
        EXPECT_TRUE(DurationConsumer::getCurrent().isInitialized());

        EXPECT_EQ(DurationConsumer::getCurrent().getSessionIdentifier().value(), "thread 2");
    });

    EXPECT_EQ(DurationConsumer::getCurrent().getSessionIdentifier().value(), "thread 1");

    thread.join();
}


TEST_F(DurationConsumerTest, DurationConsumerGuard_nestedAccess)
{
    DurationConsumerGuard guard ("test", {});

    [/* empty capture */]
    {
        EXPECT_TRUE(DurationConsumer::getCurrent().isInitialized());
        EXPECT_EQ(DurationConsumer::getCurrent().getSessionIdentifier().value(), "test");
    }
    (); // execute lambda to simulate an arbitrary callstack where the top-most stack frame (here the lambda) has no
        // direct access to the currently active DurationConsumer.
}


TEST_F(DurationConsumerTest, DurationConsumer)//NOLINT(readability-function-cognitive-complexity)
{
    std::atomic_size_t callCount = 0;
    std::chrono::system_clock::duration duration;
    std::string category;
    std::string description;
    std::string sessionIdentifier;
    std::unordered_map<std::string, std::string> keyValueMap;
    DurationConsumerGuard guard ("test", [&](const auto duration_, const std::string& category_, const auto& description_, const auto& sessionIdentifier_, const std::unordered_map<std::string, std::string>& keyValueMap_)
                                {
                                    ++callCount;
                                    duration = duration_;
                                    category = category_;
                                    description = description_;
                                    sessionIdentifier = sessionIdentifier_;
                                    keyValueMap = keyValueMap_;
                                });

    {
        auto keepAlive = DurationConsumer::getCurrent().getTimer("category", "test", {{"key", "value"}});
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    TVLOG(0) << description << " " << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() << "ms";

    ASSERT_EQ(callCount, 1u);
    ASSERT_GT(duration.count(), 0);
    ASSERT_EQ(category, "category");
    ASSERT_FALSE(description.empty());
    ASSERT_FALSE(sessionIdentifier.empty());
    ASSERT_EQ(description.find("failed"), std::string::npos);
    ASSERT_EQ(keyValueMap.size(), 1);
    ASSERT_EQ(keyValueMap.begin()->first, "key");
    ASSERT_EQ(keyValueMap.begin()->second, "value");
}


TEST_F(DurationConsumerTest, DurationConsumer_uncaughtException)
{
    std::atomic_size_t callCount = 0;
    std::chrono::system_clock::duration duration;
    std::string category;
    std::string description;
    std::string sessionIdentifier;
    std::unordered_map<std::string, std::string> keyValueMap;
    DurationConsumerGuard guard ("test", [&](const auto duration_, const std::string& category_, const auto& description_, const auto& sessionIdentifier_, const std::unordered_map<std::string, std::string>& keyValueMap_)
                                {
                                    ++callCount;
                                    duration = duration_;
                                    category = category_;
                                    description = description_;
                                    sessionIdentifier = sessionIdentifier_;
                                    keyValueMap = keyValueMap_;
                                });

    try
    {
        auto keepAlive = DurationConsumer::getCurrent().getTimer("category", "test", {{"key", "value"}});
        // Expect this exception to be picked up by the DurationTimer and turned into some kind of failure message.
        throw std::runtime_error("exception details");
    }
    catch(const std::exception&)
    {
    }

    TVLOG(0) << description << " " << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() << "ms";

    ASSERT_EQ(callCount, 1u);
    ASSERT_GT(duration.count(), 0);
    ASSERT_EQ(category, "category");
    ASSERT_FALSE(sessionIdentifier.empty());
    ASSERT_NE(description.find("failed"), std::string::npos)
        << "exception is not recogonized and reported";
    ASSERT_EQ(keyValueMap.size(), 1);
    ASSERT_EQ(keyValueMap.begin()->first, "key");
    ASSERT_EQ(keyValueMap.begin()->second, "value");
}
