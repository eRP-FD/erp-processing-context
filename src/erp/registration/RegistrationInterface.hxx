/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_REGISTRATION_REGISTRATIONINTERFACE_HXX
#define ERP_PROCESSING_CONTEXT_REGISTRATION_REGISTRATIONINTERFACE_HXX


class RegistrationInterface
{
public:
    virtual ~RegistrationInterface() = default;

    virtual void registration() = 0;
    virtual void deregistration() = 0;
    virtual void heartbeat() = 0;
};


#endif
