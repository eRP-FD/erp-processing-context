/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
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
    ApplicationHealthAndRegistrationUpdater(const std::chrono::steady_clock::duration interval,
                                            const std::chrono::steady_clock::duration retryInterval,
                                            PcServiceContext& serviceContext);
    ~ApplicationHealthAndRegistrationUpdater() noexcept override;

    static std::unique_ptr<ApplicationHealthAndRegistrationUpdater> create (
        const Configuration& configuration,
        PcServiceContext& serviceContext);

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
