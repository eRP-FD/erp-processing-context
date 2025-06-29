/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_UTIL_ENVIRONMENTVARIABLEGUARD_HXX
#define ERP_PROCESSING_CONTEXT_TEST_UTIL_ENVIRONMENTVARIABLEGUARD_HXX

#include "shared/util/Configuration.hxx"
#include "shared/util/Environment.hxx"
#include "test/util/TestConfiguration.hxx"

#include <boost/noncopyable.hpp>
#include <string>
#include <vector>

class EnvironmentVariableGuard : private boost::noncopyable
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

    EnvironmentVariableGuard(TestConfigurationKey key, const std::optional<std::string>& value)
        : EnvironmentVariableGuard(TestConfiguration::instance().getEnvironmentVariableName(key), value)
    {
    }

    ~EnvironmentVariableGuard()
    {
        if (mRestoreOldValue)
        {
            set(mPreviousValue);
        }
    }

    EnvironmentVariableGuard(EnvironmentVariableGuard&& other) noexcept
    {
        std::swap(mVariableName, other.mVariableName);
        std::swap(mPreviousValue, other.mPreviousValue);
        other.mRestoreOldValue = false;
    }

    EnvironmentVariableGuard& operator=(EnvironmentVariableGuard&& other) noexcept
    {
        if (&other != this)
        {
            std::swap(mVariableName, other.mVariableName);
            std::swap(mPreviousValue, other.mPreviousValue);
            other.mRestoreOldValue = false;
            mRestoreOldValue = true;
        }
        return *this;
    }


private:
    void set(const std::optional<std::string>& value)
    {
        if (value)
            Environment::set(mVariableName, *value);
        else
            Environment::unset(mVariableName);
    }
    bool mRestoreOldValue{true};
    std::string mVariableName;
    std::optional<std::string> mPreviousValue;
};

template <std::vector<EnvironmentVariableGuard> EnvFunctionT()>
class GuardWithEnvFrom : std::vector<EnvironmentVariableGuard> {
public:
    GuardWithEnvFrom(): vector{EnvFunctionT()}{}
};


#endif
