/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/DurationConsumer.hxx"
#include "shared/deprecated/TimerJobBase.hxx"
#include "shared/util/PeriodicTimer.hxx"
#include "shared/util/TLog.hxx"

#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <regex>
#include <thread>

static constexpr std::chrono::milliseconds interval(200);


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
    DurationConsumerGuard guard("test", [&](const auto duration_, const std::string& category_,
                                            const auto& description_, const auto& sessionIdentifier_,
                                            const std::unordered_map<std::string, std::string>& keyValueMap_,
                                            const std::optional<JsonLog::LogReceiver>&) {
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
    DurationConsumerGuard guard("test", [&](const auto duration_, const std::string& category_,
                                            const auto& description_, const auto& sessionIdentifier_,
                                            const std::unordered_map<std::string, std::string>& keyValueMap_,
                                            const std::optional<JsonLog::LogReceiver>&) {
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
    catch (const std::exception&)
    {
    }

    TVLOG(0) << description << " " << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() << "ms";

    ASSERT_EQ(callCount, 1u);
    ASSERT_GT(duration.count(), 0);
    ASSERT_EQ(category, "category");
    ASSERT_FALSE(sessionIdentifier.empty());
    EXPECT_EQ(description, "test");
    ASSERT_EQ(keyValueMap.size(), 2);
    ASSERT_TRUE(keyValueMap.contains("key"));
    EXPECT_EQ(keyValueMap.at("key"), "value");
    ASSERT_TRUE(keyValueMap.contains("error")) << "exception is not recogonized and reported";
    EXPECT_EQ(keyValueMap.at("error"), "uncaught exception");
}

TEST_F(DurationConsumerTest, TimerJob)
{
    testing::internal::CaptureStderr();

    class TestTimer : public TimerJobBase {
        public:
        TestTimer()
            : TimerJobBase{"DurationConsumerTest", interval}
        {
        }
        ~TestTimer() override = default;
        void onStart() override {}
        void executeJob() override
        {
            ::DurationConsumer::getCurrent().getTimer("DurationConsumerTest", "TEST:executeJob");
        }
        void onFinish() override {}
    };
    TestTimer testTimer;
    testTimer.start();
    std::this_thread::sleep_for(interval * 2);
    testTimer.shutdown();

    std::string output = testing::internal::GetCapturedStderr();
    TLOG(INFO) << output;
    ASSERT_TRUE(output.find("TEST:executeJob") != std::string::npos);
    ASSERT_TRUE(output.find("error") == std::string::npos);
    ASSERT_TRUE(output.find("fail") == std::string::npos);
}

TEST_F(DurationConsumerTest, PeriodicTimer)
{
    testing::internal::CaptureStderr();

    class TestTimerHandler : public FixedIntervalHandler {
        using FixedIntervalHandler::FixedIntervalHandler;
        void timerHandler() override
        {
            ::DurationConsumer::getCurrent().getTimer("DurationConsumerTest", "TEST:timerHandler");
        }
    };
    using TestTimer = PeriodicTimer<TestTimerHandler>;
    boost::asio::io_context ioContext;
    TestTimer testTimer(interval);
    testTimer.start(ioContext, interval);
    ioContext.run_for(interval * 2);
    ioContext.stop();

    std::string output = testing::internal::GetCapturedStderr();
    TLOG(INFO) << output;
    ASSERT_TRUE(output.find("TEST:timerHandler") != std::string::npos);
    ASSERT_TRUE(output.find("error") == std::string::npos);
    ASSERT_TRUE(output.find("fail") == std::string::npos);
}

TEST_F(DurationConsumerTest, Blockduration)
{
   testing::internal::CaptureStderr();

    {
        int session = 0;
        DurationConsumerGuard durationConsumerGuard(
            "Block duration",
            [&session](const std::chrono::steady_clock::duration /*duration*/, const std::string& /*category*/,
                            const std::string& /*description*/, const std::string& /*sessionIdentifier*/,
                            const std::unordered_map<std::string, std::string>& /*keyValueMap*/,
                            const std::optional<JsonLog::LogReceiver>& /*logReceiverOverride*/) {
                session += 1;
            });
        ::DurationConsumer::getCurrent().getTimer("DurationConsumerTest", "TEST:Blockduration");
        ASSERT_EQ(session, 1);
    }

    std::string output = testing::internal::GetCapturedStderr();
    TLOG(INFO) << output;
    ASSERT_TRUE(output.find("TEST:Blockduration") != std::string::npos);
    ASSERT_TRUE(output.find("error") == std::string::npos);
    ASSERT_TRUE(output.find("fail") == std::string::npos);
}

TEST_F(DurationConsumerTest, DurationTimerExpectedFormat)
{
    std::string expected =
        R"({"log_type":"timing","x_request_id":"d85f6714-35f7-400e-9d17-52bd086b72b7","category":"fhirvalidation","metric":"GEM_ERP_PR_PAR_CloseOperation_Input","profile_version":"1.4","duration_us":)";

    {
        DurationConsumerGuard guard("d85f6714-35f7-400e-9d17-52bd086b72b7",
                                    [&](const auto, const std::string&, const auto&, const auto&,
                                        const std::unordered_map<std::string, std::string>&,
                                        const std::optional<JsonLog::LogReceiver>&) {
                                    });
        testing::internal::CaptureStderr();
        DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryFhirValidation, "GEM_ERP_PR_PAR_CloseOperation_Input", {{"profile_version", "1.4"}});
    }
    std::string output = testing::internal::GetCapturedStderr();
    TLOG(INFO) << output;
    ASSERT_NE(output.find(expected), std::string::npos)  << output;
}
TEST_F(DurationConsumerTest, DurationTimerFailedExpectedFormat)
{
    std::string expected =
        R"({"log_type":"timing","x_request_id":"d85f6714-35f7-400e-9d17-52bd086b72b7","category":"fhirvalidation","metric":"GEM_ERP_PR_PAR_CloseOperation_Input","error":"uncaught exception","profile_version":"1.4","duration_us":)";

    {
        DurationConsumerGuard guard("d85f6714-35f7-400e-9d17-52bd086b72b7",
                                    [&](const auto, const std::string&, const auto&, const auto&,
                                        const std::unordered_map<std::string, std::string>&,
                                        const std::optional<JsonLog::LogReceiver>&) {
                                    });
        testing::internal::CaptureStderr();
        try
        {
            auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryFhirValidation,
                                                                 "GEM_ERP_PR_PAR_CloseOperation_Input",
                                                                 {{"profile_version", "1.4"}});
            throw std::runtime_error("fhir validation failed");
        }
        catch (const std::exception&)
        {
        }
    }
    std::string output = testing::internal::GetCapturedStderr();
    TLOG(INFO) << output;
    ASSERT_NE(output.find(expected), std::string::npos) << output;
}
