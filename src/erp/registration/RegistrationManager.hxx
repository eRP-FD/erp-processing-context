/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_REGISTRATION_REGISTRATIONCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_REGISTRATION_REGISTRATIONCLIENT_HXX

#include "erp/registration/RegistrationInterface.hxx"
#include "erp/service/RedisInterface.hxx"
#include "erp/util/Configuration.hxx"


class RegistrationManager : public RegistrationInterface
{
public:
    static std::unique_ptr<RegistrationInterface> createFromConfig(
        const std::uint16_t aTeePort,
        const Configuration& configuration);

    RegistrationManager(
        const std::string& aTeeHost,
        const std::uint16_t aTeePort,
        std::unique_ptr<RedisInterface> aRedisInterface);

    RegistrationManager(RegistrationManager&& other) noexcept;

    void registration() override;

    void deregistration() override;

    void heartbeat() override;

private:
    const std::string mRedisKey;
    std::unique_ptr<RedisInterface> mRedisInterface;
};


#endif
