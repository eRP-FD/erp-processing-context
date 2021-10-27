/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_UTIL_ENVIRONMENTVARIABLEGUARD_HXX
#define ERP_PROCESSING_CONTEXT_TEST_UTIL_ENVIRONMENTVARIABLEGUARD_HXX

#include "erp/util/Environment.hxx"

#include <string>

class EnvironmentVariableGuard
{
public:
    EnvironmentVariableGuard(const std::string& variableName, const std::string& value)
        : mVariableName(variableName)
        , mPreviousValue(Environment::get(variableName))
    {
        Environment::set(variableName, value);
    }

    ~EnvironmentVariableGuard()
    {
        if (mPreviousValue)
            Environment::set(mVariableName, *mPreviousValue);
        else
            Environment::unset(mVariableName);
    }

private:
    const std::string mVariableName;
    std::optional<std::string> mPreviousValue;
};


#endif
