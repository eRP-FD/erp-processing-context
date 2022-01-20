/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/registration/HeartbeatSender.hxx"
#include "erp/util/TLog.hxx"
#include "test/mock/MockTerminationHandler.hxx"
#include "erp/util/health/ApplicationHealth.hxx"
#include "test/mock/RegistrationMock.hxx"

#include <gtest/gtest.h>

#include <thread> // for std::this_thread::sleep_for
#include <string>


class HeartbeatSenderTest : public testing::Test
{
public:
    void SetUp() override
    {
        MockTerminationHandler::setupForTesting();
    }

protected:
    void TearDown() override
    {
        MockTerminationHandler::setupForTesting();
    }
};

TEST_F(HeartbeatSenderTest, Success)
{
    auto uniqueRegistrationManager = std::make_shared<RegistrationMock>();
    auto& registrationManager = *uniqueRegistrationManager;
    ApplicationHealth applicationHealth;
    auto sender =
        std::make_unique<HeartbeatSender>(std::move(uniqueRegistrationManager), std::chrono::milliseconds(1500),
                                          std::chrono::seconds(1), applicationHealth);

    sender->start();
    std::this_thread::sleep_for(std::chrono::seconds(4));
    MockTerminationHandler::instance().notifyTerminationCallbacks(false);
    dynamic_cast<MockTerminationHandler&>(MockTerminationHandler::instance()).waitForTerminated();

    ASSERT_EQ(registrationManager.currentState(), RegistrationMock::State::deregistered);
}

TEST_F(HeartbeatSenderTest, FailStartup)
{
    auto uniqueRegistrationManager = std::make_shared<RegistrationMock>();
    auto& registrationManager = *uniqueRegistrationManager;
    ApplicationHealth applicationHealth;
    auto sender = std::make_unique<HeartbeatSender>(
        std::move(uniqueRegistrationManager), std::chrono::seconds(1), std::chrono::seconds(1), applicationHealth);

    registrationManager.setSimulateServerDown(true);
    sender->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    ASSERT_EQ(registrationManager.currentState(), RegistrationMock::State::initialized);
}

TEST_F(HeartbeatSenderTest, FailDeregistration)
{
    auto uniqueRegistrationManager = std::make_shared<RegistrationMock>();
    auto& registrationManager = *uniqueRegistrationManager;
    ApplicationHealth applicationHealth;
    auto sender = std::make_unique<HeartbeatSender>(
        std::move(uniqueRegistrationManager), std::chrono::seconds(20), std::chrono::seconds(10), applicationHealth);

    sender->start();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    registrationManager.setSimulateServerDown(true);
    MockTerminationHandler::instance().notifyTerminationCallbacks(false);
    dynamic_cast<MockTerminationHandler&>(MockTerminationHandler::instance()).waitForTerminated();

    ASSERT_TRUE(registrationManager.deregistrationCalled());
    ASSERT_EQ(registrationManager.currentState(), RegistrationMock::State::registered);
}
