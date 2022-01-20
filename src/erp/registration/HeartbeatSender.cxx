/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/registration/HeartbeatSender.hxx"
#include "erp/registration/RegistrationManager.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TerminationHandler.hxx"
#include "erp/util/health/ApplicationHealth.hxx"

#include <chrono>
#include <iostream>


std::unique_ptr<HeartbeatSender> HeartbeatSender::create(const Configuration& configuration,
                                                         const ApplicationHealth& applicationHealth,
                                                         std::shared_ptr<RegistrationInterface> registrationInterface)
{
    Expect(registrationInterface, "can not create HeartbeatSender without registration");
    const auto interval = std::chrono::seconds(
        configuration.getOptionalIntValue(ConfigurationKey::REGISTRATION_HEARTBEAT_INTERVAL_SEC, 30));
    // Conceptually the time between two retry calls is different from the time between two successful
    // heartbeat calls. But as long as we have not agreed on a different value, we will use the same.
    const auto retryInterval = interval;

    return std::make_unique<HeartbeatSender>(std::move(registrationInterface), interval, retryInterval, applicationHealth);
}


HeartbeatSender::HeartbeatSender(std::shared_ptr<RegistrationInterface> registration,
                                 const std::chrono::steady_clock::duration interval,
                                 const std::chrono::steady_clock::duration retryInterval,
                                 const ApplicationHealth& applicationHealth)
    : TimerJobBase("heartbeat", interval)
    , mRegistration(std::move(registration))
    , mRetryInterval(retryInterval)
    , mApplicationHealth(applicationHealth)
{
    Expect(mRegistration, "can not create HeartbeatSender without registration");
}


void HeartbeatSender::onStart(void)
{
    mRegistration->updateRegistrationBasedOnApplicationHealth(mApplicationHealth);

    if (mRegistration->registered())
    {
        // first execution is done immediately
        executeJob();
    }
}


void HeartbeatSender::executeJob(void)
{
    mRegistration->updateRegistrationBasedOnApplicationHealth(mApplicationHealth);
    if (!mRegistration->registered())
    {
        return;
    }

    size_t tryCount = 0;
    while (! isAborted())
    {
        try
        {
            ++tryCount;
            mRegistration->heartbeat();

            // If we get here the call was successful and we don't have to retry.
            return;
        }
        catch (const std::exception& e)
        {
            LOG(ERROR) << "Heartbeat failed in attempt # " << tryCount << " with " << e.what() << std::endl;
            LOG(ERROR) << "waiting for "
                       << static_cast<double>(
                              std::chrono::duration_cast<std::chrono::milliseconds>(mRetryInterval).count()) /
                              1000.0
                       << " seconds before trying again" << std::endl;
        }

        // Before we retry, wait some time.
        waitFor(mRetryInterval);
    }
}


void HeartbeatSender::onFinish(void)
{
    try
    {
        mRegistration->deregistration();
    }
    catch (std::exception& e)
    {
        LOG(ERROR) << "heartbeat sender deregistration failed: " << e.what();
        // Do nothing. Deregistering the heartbeat sender is part of the application shutting down.
        // Therefore there is nothing else we can do about this error.
    }
    catch (...)
    {
        LOG(ERROR) << "heartbeat sender deregistration failed";
        // Do nothing. Deregistering the heartbeat sender is part of the application shutting down.
        // Therefore there is nothing else we can do about this error.
    }
}
