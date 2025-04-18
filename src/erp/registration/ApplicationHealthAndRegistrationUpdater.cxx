/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/registration/ApplicationHealthAndRegistrationUpdater.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/registration/RegistrationManager.hxx"
#include "erp/util/health/HealthCheck.hxx"
#include "shared/deprecated/TerminationHandler.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/health/ApplicationHealth.hxx"

#include <chrono>
#include <iostream>

#include <sw/redis++/errors.h>


std::unique_ptr<ApplicationHealthAndRegistrationUpdater>
ApplicationHealthAndRegistrationUpdater::create(const Configuration& configuration,
                                                         PcServiceContext& serviceContext,
                                                         std::shared_ptr<RegistrationInterface> registrationInterface)
{
    Expect(registrationInterface, "can not create ApplicationHealthAndRegistrationUpdater without registration");
    const auto interval = std::chrono::seconds(
        configuration.getOptionalIntValue(ConfigurationKey::REGISTRATION_HEARTBEAT_INTERVAL_SEC, 30));
    // Conceptually the time between two retry calls is different from the time between two successful
    // heartbeat calls. But as long as we have not agreed on a different value, we will use the same.
    const auto retryInterval = interval;

    return std::make_unique<ApplicationHealthAndRegistrationUpdater>(std::move(registrationInterface), interval, retryInterval, serviceContext);
}


ApplicationHealthAndRegistrationUpdater::ApplicationHealthAndRegistrationUpdater(std::shared_ptr<RegistrationInterface> registration,
                                 const std::chrono::steady_clock::duration interval,
                                 const std::chrono::steady_clock::duration retryInterval,
                                 PcServiceContext& serviceContext)
    : TimerJobBase("heartbeat", interval)
    , mRegistration(std::move(registration))
    , mRetryInterval(retryInterval)
    , mServiceContext(serviceContext)
{
    Expect(mRegistration, "can not create ApplicationHealthAndRegistrationUpdater without registration");
}


void ApplicationHealthAndRegistrationUpdater::onStart(void)
{
    HealthCheck::update(mServiceContext);
    try
    {
        mRegistration->updateRegistrationBasedOnApplicationHealth(mServiceContext.applicationHealth());
        if (mRegistration->registered())
        {
            // first execution is done immediately
            executeJob();
        }
    }
    catch (const sw::redis::Error& e)
    {
        TLOG(ERROR) << "Redis error while updating registration info during start, with " << e.what();
    }
}


void ApplicationHealthAndRegistrationUpdater::executeJob(void)
{
    HealthCheck::update(mServiceContext);
    bool registered = false;
    try
    {
        mRegistration->updateRegistrationBasedOnApplicationHealth(mServiceContext.applicationHealth());
        registered = mRegistration->registered();
    }
    catch (const sw::redis::Error& e)
    {
        TLOG(ERROR) << "Redis error while updating registration info with " << e.what();
    }

    if (!registered)
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

            // If we get here the call was successful, we don't have to try again.
            return;
        }
        catch (const std::exception& e)
        {
            TLOG(ERROR) << "Heartbeat failed in attempt # " << tryCount << " with " << e.what();
            TLOG(ERROR) << "waiting for "
                       << static_cast<double>(
                              std::chrono::duration_cast<std::chrono::milliseconds>(mRetryInterval).count()) /
                              1000.0
                       << " seconds before trying again";
        }

        // Before we retry, wait some time.
        waitFor(mRetryInterval);
    }
}


void ApplicationHealthAndRegistrationUpdater::onFinish(void)
{
    try
    {
        mRegistration->deregistration();
    }
    catch (const std::exception& e)
    {
        TLOG(ERROR) << "heartbeat sender deregistration failed: " << e.what();
        // Do nothing. Deregistering the heartbeat sender is part of the application shutting down.
        // Therefore there is nothing else we can do about this error.
    }
    catch (...)
    {
        TLOG(ERROR) << "heartbeat sender deregistration failed";
        // Do nothing. Deregistering the heartbeat sender is part of the application shutting down.
        // Therefore there is nothing else we can do about this error.
    }
}
