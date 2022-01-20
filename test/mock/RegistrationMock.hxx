/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_MOCK_REGISTRATIONMOCK_HXX
#define ERP_PROCESSING_CONTEXT_TEST_MOCK_REGISTRATIONMOCK_HXX


#include "erp/registration/RegistrationInterface.hxx"

class RegistrationMock : public RegistrationInterface
{
public:
    enum class State
    {
        initialized,
        registered,
        deregistered
    };

    State currentState() const;

    unsigned int heartbeatRetryNum() const;

    bool deregistrationCalled() const;

    void registration() override;

    void deregistration() override;

    void heartbeat() override;

    void setSimulateServerDown(bool flag);

    bool registered() const override;

    void updateRegistrationBasedOnApplicationHealth(const ApplicationHealth& applicationHealth) override;

private:
    State mCurrentState = State::initialized;
    bool mSimulateServerDown = false;
    unsigned int mHeartbeatRetryNum = 0;
    bool mDeregistrationCalled = false;
};


#endif//ERP_PROCESSING_CONTEXT_TEST_MOCK_REGISTRATIONMOCK_HXX
