/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_UTIL_ENVIRONMENTVARIABLEGUARD_HXX
#define ERP_PROCESSING_CONTEXT_TEST_UTIL_ENVIRONMENTVARIABLEGUARD_HXX

#include "erp/util/Environment.hxx"

#include "erp/util/Configuration.hxx"

#include <string>

class EnvironmentVariableGuard
{
public:
    EnvironmentVariableGuard(const std::string& variableName, const std::optional<std::string>& value)
        : mVariableName(variableName)
        , mPreviousValue(Environment::get(variableName))
    {
        set(value);
    }

    EnvironmentVariableGuard(ConfigurationKey key, const std::optional<std::string>& value)
        : EnvironmentVariableGuard(Configuration::instance().getEnvironmentVariableName(key), value)
    {
    }

    ~EnvironmentVariableGuard()
    {
        set(mPreviousValue);
    }

private:
    void set(const std::optional<std::string>& value)
    {
        if (value)
            Environment::set(mVariableName, *value);
        else
            Environment::unset(mVariableName);
    }

    const std::string mVariableName;
    std::optional<std::string> mPreviousValue;
};


#endif
