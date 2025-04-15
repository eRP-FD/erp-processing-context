/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_REGISTRATION_REGISTRATIONINTERFACE_HXX
#define ERP_PROCESSING_CONTEXT_REGISTRATION_REGISTRATIONINTERFACE_HXX

class ApplicationHealth;

class RegistrationInterface
{
public:
    virtual ~RegistrationInterface() = default;

    virtual void registration() = 0;
    virtual void deregistration() = 0;
    virtual void heartbeat() = 0;
    virtual bool registered() const = 0;
    virtual void updateRegistrationBasedOnApplicationHealth(const ApplicationHealth& applicationHealth) = 0;
};


#endif
