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

class ApplicationHealth;

// Administration of Heartbeat sender thread.

class HeartbeatSender : public TimerJobBase
{
public:
    HeartbeatSender(
        std::shared_ptr<RegistrationInterface> registration,
        const std::chrono::steady_clock::duration interval,
        const std::chrono::steady_clock::duration retryInterval,
        const ApplicationHealth& applicationHealth);
    ~HeartbeatSender() noexcept override = default;

    static std::unique_ptr<HeartbeatSender> create (
        const Configuration& configuration,
        const ApplicationHealth& applicationHealth,
        std::shared_ptr<RegistrationInterface> registrationInterface);

protected:
    void onStart(void) override;
    void executeJob(void) override;
    void onFinish(void) override;

private:
    std::shared_ptr<RegistrationInterface> mRegistration;
    const std::chrono::steady_clock::duration mRetryInterval;
    const ApplicationHealth& mApplicationHealth;
};


#endif
