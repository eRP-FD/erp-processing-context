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


// Administration of Heartbeat sender thread.

class HeartbeatSender : public TimerJobBase
{
public:
    HeartbeatSender(
        std::unique_ptr<RegistrationInterface> registration,
        const std::chrono::steady_clock::duration interval,
        const std::chrono::steady_clock::duration retryInterval);
    ~HeartbeatSender() noexcept override = default;

    using RegistrationFactory = std::function<std::unique_ptr<RegistrationInterface>(const uint16_t, const Configuration&)>;

    static std::unique_ptr<HeartbeatSender> create (
        uint16_t teePort,
        const Configuration& configuration,
        RegistrationFactory&& registrationFactory = createDefaultRegistrationFactory());

    static RegistrationFactory createDefaultRegistrationFactory (void);

protected:
    void onStart(void) override;
    void executeJob(void) override;
    void onFinish(void) override;

private:
    std::unique_ptr<RegistrationInterface> mRegistration;
    const std::chrono::steady_clock::duration mRetryInterval;
};


#endif
