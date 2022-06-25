/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_REGISTRATION_HEARTBEATSENDER_HXX
#define ERP_PROCESSING_CONTEXT_REGISTRATION_HEARTBEATSENDER_HXX

#include "erp/registration/RegistrationInterface.hxx"
#include "erp/util/TimerJobBase.hxx"
#include "erp/util/Configuration.hxx"

#include <future>
#include <memory>
#include <mutex>
#include <thread>

class PcServiceContext;

// Administration of Heartbeat sender thread.

class ApplicationHealthAndRegistrationUpdater : public TimerJobBase
{
public:
    ApplicationHealthAndRegistrationUpdater(
        std::shared_ptr<RegistrationInterface> registration,
        const std::chrono::steady_clock::duration interval,
        const std::chrono::steady_clock::duration retryInterval,
        PcServiceContext& serviceContext);
    ~ApplicationHealthAndRegistrationUpdater() noexcept override = default;

    static std::unique_ptr<ApplicationHealthAndRegistrationUpdater> create (
        const Configuration& configuration,
        PcServiceContext& serviceContext,
        std::shared_ptr<RegistrationInterface> registrationInterface);

protected:
    void onStart(void) override;
    void executeJob(void) override;
    void onFinish(void) override;

private:
    std::shared_ptr<RegistrationInterface> mRegistration;
    const std::chrono::steady_clock::duration mRetryInterval;
    PcServiceContext& mServiceContext;
};


#endif
