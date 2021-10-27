/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/registration/HeartbeatSender.hxx"
#include "erp/util/TLog.hxx"
#include "test/mock/MockTerminationHandler.hxx"

#include <gtest/gtest.h>

#include <thread> // for std::this_thread::sleep_for
#include <string>


class RegistrationMock : public RegistrationInterface
{
public:
    enum class State
    {
        initialized,
        registered,
        deregistered
    };

    State currentState() const
    {
        return mCurrentState;
    }

    unsigned int heartbeatRetryNum() const
    {
        return mHeartbeatRetryNum;
    }

    bool deregistrationCalled() const
    {
        return mDeregistrationCalled;
    }

    void registration() override
    {
        TVLOG(1) << "registration";
        ASSERT_TRUE(mCurrentState == State::initialized || mCurrentState == State::deregistered);
        if(mSimulateServerDown)
            throw std::runtime_error("Connection refused");
        mCurrentState = State::registered;
    }

    void deregistration() override
    {
        TVLOG(1) << "deregistration";
        mDeregistrationCalled = true;
        ASSERT_EQ(mCurrentState, State::registered);
        if(mSimulateServerDown)
            throw std::runtime_error("Connection closed");
        mCurrentState = State::deregistered;
    }

    void heartbeat() override
    {
        TVLOG(1) << "heartbeat";
        ASSERT_EQ(mCurrentState, State::registered);
        if(mSimulateServerDown)
        {
            ++mHeartbeatRetryNum;
            throw std::runtime_error("Connection closed");
        }
        mHeartbeatRetryNum = 0;
    }

    void setSimulateServerDown(bool flag)
    {
        mSimulateServerDown = flag;
    }

private:
    State mCurrentState = State::initialized;
    bool mSimulateServerDown = false;
    unsigned int mHeartbeatRetryNum = 0;
    bool mDeregistrationCalled = false;
};


class HeartbeatSenderTest : public testing::Test
{
public:
    void SetUp() override
    {
        MockTerminationHandler::setupForTesting();
    }
};

TEST_F(HeartbeatSenderTest, Success)
{
    auto uniqueRegistrationManager = std::make_unique<RegistrationMock>();
    auto& registrationManager = *uniqueRegistrationManager;
    auto sender = std::make_unique<HeartbeatSender>(
        std::move(uniqueRegistrationManager), std::chrono::milliseconds(1500), std::chrono::seconds(1));

    sender->start();
    std::this_thread::sleep_for(std::chrono::seconds(4));
    MockTerminationHandler::instance().notifyTerminationCallbacks(false);
    dynamic_cast<MockTerminationHandler&>(MockTerminationHandler::instance()).waitForTerminated();

    ASSERT_EQ(registrationManager.currentState(), RegistrationMock::State::deregistered);
}

TEST_F(HeartbeatSenderTest, FailStartup)
{
    auto uniqueRegistrationManager = std::make_unique<RegistrationMock>();
    auto& registrationManager = *uniqueRegistrationManager;
    auto sender = std::make_unique<HeartbeatSender>(
        std::move(uniqueRegistrationManager), std::chrono::seconds(1), std::chrono::seconds(1));

    registrationManager.setSimulateServerDown(true);
    sender->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    ASSERT_EQ(registrationManager.currentState(), RegistrationMock::State::initialized);
}

TEST_F(HeartbeatSenderTest, FailDeregistration)
{
    auto uniqueRegistrationManager = std::make_unique<RegistrationMock>();
    auto& registrationManager = *uniqueRegistrationManager;
    auto sender = std::make_unique<HeartbeatSender>(
        std::move(uniqueRegistrationManager), std::chrono::seconds(20), std::chrono::seconds(10));

    sender->start();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    registrationManager.setSimulateServerDown(true);
    MockTerminationHandler::instance().notifyTerminationCallbacks(false);
    dynamic_cast<MockTerminationHandler&>(MockTerminationHandler::instance()).waitForTerminated();

    ASSERT_TRUE(registrationManager.deregistrationCalled());
    ASSERT_EQ(registrationManager.currentState(), RegistrationMock::State::registered);
}
