/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/registration/ApplicationHealthAndRegistrationUpdater.hxx"
#include "erp/util/TLog.hxx"
#include "test/mock/MockTerminationHandler.hxx"
#include "erp/util/health/ApplicationHealth.hxx"
#include "test/mock/RegistrationMock.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>

#include <thread> // for std::this_thread::sleep_for
#include <string>

class PcServiceContextWithMockRegistrationManager : public PcServiceContext
{
public:
    PcServiceContextWithMockRegistrationManager(const Configuration& configuration, Factories&& factories)
        : PcServiceContext(configuration, std::move(factories))
    {
    }

    std::shared_ptr<RegistrationInterface> registrationInterface() const override
    {
        return mRegistrationInterface;
    }

    std::shared_ptr<RegistrationMock> mockRegistrationInterface() const
    {
        return mRegistrationInterface;
    }

private:
    std::shared_ptr<RegistrationMock> mRegistrationInterface = std::make_shared<RegistrationMock>();
};


class ApplicationHealthAndRegistrationUpdaterTest : public testing::Test
{
public:
    void SetUp() override
    {
        MockTerminationHandler::setupForTesting();
    }

    static PcServiceContextWithMockRegistrationManager makeServiceContext()
    {
        return PcServiceContextWithMockRegistrationManager(Configuration::instance(), StaticData::makeMockFactories());
    }

protected:
    void TearDown() override
    {
        MockTerminationHandler::setupForTesting();
    }
};


TEST_F(ApplicationHealthAndRegistrationUpdaterTest, Success)
{
    auto serviceContext{makeServiceContext()};
    auto sender =
        std::make_unique<ApplicationHealthAndRegistrationUpdater>(std::chrono::milliseconds(1500),
                                          std::chrono::seconds(1), serviceContext);

    sender->start();
    std::this_thread::sleep_for(std::chrono::seconds(4));
    MockTerminationHandler::instance().notifyTerminationCallbacks(false);
    dynamic_cast<MockTerminationHandler&>(MockTerminationHandler::instance()).waitForTerminated();

    ASSERT_EQ(serviceContext.mockRegistrationInterface()->currentState(), RegistrationMock::State::deregistered);
}

TEST_F(ApplicationHealthAndRegistrationUpdaterTest, FailStartup)
{
    auto serviceContext{makeServiceContext()};
    auto sender = std::make_unique<ApplicationHealthAndRegistrationUpdater>(std::chrono::seconds(1),
                                                                            std::chrono::seconds(1), serviceContext);

    serviceContext.mockRegistrationInterface()->setSimulateServerDown(true);
    sender->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    ASSERT_EQ(serviceContext.mockRegistrationInterface()->currentState(), RegistrationMock::State::initialized);

    serviceContext.mockRegistrationInterface()->setSimulateServerDown(false);
    serviceContext.mockRegistrationInterface()->registration();
}

TEST_F(ApplicationHealthAndRegistrationUpdaterTest, FailDeregistration)
{
    auto serviceContext{makeServiceContext()};
    auto sender = std::make_unique<ApplicationHealthAndRegistrationUpdater>(std::chrono::seconds(20),
                                                                            std::chrono::seconds(10), serviceContext);

    sender->start();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    serviceContext.mockRegistrationInterface()->setSimulateServerDown(true);
    MockTerminationHandler::instance().notifyTerminationCallbacks(false);
    dynamic_cast<MockTerminationHandler&>(MockTerminationHandler::instance()).waitForTerminated();

    ASSERT_TRUE(serviceContext.mockRegistrationInterface()->deregistrationCalled());
    ASSERT_EQ(serviceContext.mockRegistrationInterface()->currentState(), RegistrationMock::State::registered);
}
