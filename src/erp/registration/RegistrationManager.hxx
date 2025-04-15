/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_REGISTRATION_REGISTRATIONCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_REGISTRATION_REGISTRATIONCLIENT_HXX

#include "erp/registration/RegistrationInterface.hxx"
#include "erp/service/RedisInterface.hxx"
#include "shared/util/Configuration.hxx"

class ApplicationHealth;

class RegistrationManager : public RegistrationInterface
{
public:
    RegistrationManager(
        const std::string& aTeeHost,
        const std::uint16_t aTeePort,
        std::unique_ptr<RedisInterface> aRedisInterface);

    RegistrationManager(RegistrationManager&& other) noexcept;

    void registration() override;

    void deregistration() override;

    void heartbeat() override;

    bool registered() const override;

    void updateRegistrationBasedOnApplicationHealth(const ApplicationHealth& applicationHealth) override;

private:
    mutable std::mutex mMutex;
    const std::string mRedisKey;
    std::unique_ptr<RedisInterface> mRedisInterface;
};


#endif
