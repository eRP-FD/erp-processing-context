/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/registration/ApplicationHealthAndRegistrationUpdater.hxx"
#include "shared/util/TLog.hxx"
#include "shared/util/health/ApplicationHealth.hxx"
#include "test/mock/MockTerminationHandler.hxx"
#include "test/mock/RegistrationMock.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>
#include <string>
#include <thread>// for std::this_thread::sleep_for


class ApplicationHealthAndRegistrationUpdaterTest : public testing::Test
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

TEST_F(ApplicationHealthAndRegistrationUpdaterTest, Success)
{
    auto uniqueRegistrationManager = std::make_shared<RegistrationMock>();
    auto& registrationManager = *uniqueRegistrationManager;
    auto serviceContext = StaticData::makePcServiceContext();
    auto sender =
        std::make_unique<ApplicationHealthAndRegistrationUpdater>(std::move(uniqueRegistrationManager), std::chrono::milliseconds(1500),
                                          std::chrono::seconds(1), serviceContext);

    sender->start();
    std::this_thread::sleep_for(std::chrono::seconds(4));
    MockTerminationHandler::instance().notifyTerminationCallbacks(false);
    dynamic_cast<MockTerminationHandler&>(MockTerminationHandler::instance()).waitForTerminated();

    ASSERT_EQ(registrationManager.currentState(), RegistrationMock::State::deregistered);
}

TEST_F(ApplicationHealthAndRegistrationUpdaterTest, FailStartup)
{
    auto uniqueRegistrationManager = std::make_shared<RegistrationMock>();
    auto& registrationManager = *uniqueRegistrationManager;
    auto serviceContext = StaticData::makePcServiceContext();
    auto sender = std::make_unique<ApplicationHealthAndRegistrationUpdater>(
        std::move(uniqueRegistrationManager), std::chrono::seconds(1), std::chrono::seconds(1), serviceContext);

    registrationManager.setSimulateServerDown(true);
    sender->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    ASSERT_EQ(registrationManager.currentState(), RegistrationMock::State::initialized);
}

TEST_F(ApplicationHealthAndRegistrationUpdaterTest, FailDeregistration)
{
    auto uniqueRegistrationManager = std::make_shared<RegistrationMock>();
    auto& registrationManager = *uniqueRegistrationManager;
    auto serviceContext = StaticData::makePcServiceContext();
    auto sender = std::make_unique<ApplicationHealthAndRegistrationUpdater>(
        std::move(uniqueRegistrationManager), std::chrono::seconds(20), std::chrono::seconds(10), serviceContext);

    sender->start();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    registrationManager.setSimulateServerDown(true);
    MockTerminationHandler::instance().notifyTerminationCallbacks(false);
    dynamic_cast<MockTerminationHandler&>(MockTerminationHandler::instance()).waitForTerminated();

    ASSERT_TRUE(registrationManager.deregistrationCalled());
    ASSERT_EQ(registrationManager.currentState(), RegistrationMock::State::registered);
}
