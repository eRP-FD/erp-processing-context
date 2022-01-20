/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "RegistrationMock.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/health/ApplicationHealth.hxx"

#include <gtest/gtest.h>

RegistrationMock::State RegistrationMock::currentState() const
{
    return mCurrentState;
}
unsigned int RegistrationMock::heartbeatRetryNum() const
{
    return mHeartbeatRetryNum;
}
bool RegistrationMock::deregistrationCalled() const
{
    return mDeregistrationCalled;
}
void RegistrationMock::registration()
{
    TVLOG(1) << "registration";
    ASSERT_TRUE(mCurrentState == State::initialized || mCurrentState == State::deregistered);
    if (mSimulateServerDown)
        throw std::runtime_error("Connection refused");
    mCurrentState = State::registered;
}
void RegistrationMock::deregistration()
{
    TVLOG(1) << "deregistration";
    mDeregistrationCalled = true;
    ASSERT_EQ(mCurrentState, State::registered);
    if (mSimulateServerDown)
        throw std::runtime_error("Connection closed");
    mCurrentState = State::deregistered;
}
void RegistrationMock::heartbeat()
{
    TVLOG(1) << "heartbeat";
    ASSERT_EQ(mCurrentState, State::registered);
    if (mSimulateServerDown)
    {
        ++mHeartbeatRetryNum;
        throw std::runtime_error("Connection closed");
    }
    mHeartbeatRetryNum = 0;
}
void RegistrationMock::setSimulateServerDown(bool flag)
{
    mSimulateServerDown = flag;
}
bool RegistrationMock::registered() const
{
    return mCurrentState == State::registered;
}
void RegistrationMock::updateRegistrationBasedOnApplicationHealth(const ApplicationHealth&)
{
    if (mCurrentState != State::registered)
    {
        registration();
    }
}
