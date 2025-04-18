/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "Environment.hxx"

#include "shared/util/String.hxx"
#include "shared/util/TLog.hxx"

#include <set>

std::optional<std::string> Environment::get (const std::string& variableName)
{
    const char* value = getCharacterPointer(variableName);
    if (value == nullptr)
        return {};
    else
        return std::string(value);
}


const char* Environment::getCharacterPointer (const std::string& variableName)
{
#ifdef __USE_GNU
    return secure_getenv(variableName.c_str());
#else
    return std::getenv(variableName.c_str());
#endif
}


void Environment::set (const std::string& variableName, const std::string& value)
{
#ifdef _WINDOWS
    _putenv_s(variableName.c_str(), value.c_str());
#else
    setenv(variableName.c_str(), value.c_str(), true); //NOLINT(concurrency-mt-unsafe)
#endif
}


void Environment::unset (const std::string& variableName)
{
#ifdef _WINDOWS
    _putenv_s(variableName.c_str(), "");
#else
    unsetenv(variableName.c_str());//NOLINT(concurrency-mt-unsafe)
#endif
}


std::string Environment::getString(const char* envConfigName, const std::string_view& defaultValue)
{
    auto val =  get(envConfigName);
    if (val)
    {
        return val.value();
    }
    else
    {
        return std::string{defaultValue};
    }
}


int Environment::getInt(const char* envConfigName, const int defaultValue)
{
    const auto environmentVariable = Environment::get(envConfigName);

    if (environmentVariable.has_value())
    {
        const auto& strVal = environmentVariable.value();
        try
        {
            size_t pos = 0;
            auto val = std::stoi(strVal, &pos);
            if (pos == strVal.size())
            {
                return val;
            }
        }
        catch (const std::logic_error& ) //NOLINT(bugprone-empty-catch)
        {
            // Nothing to do here, the default value will be returned
        }
        TLOG(WARNING) << "Invalid Value in environment variable '"
                     << *environmentVariable
                     << "': '" << strVal
                     << "' - expected integer. using default: " << defaultValue;
    }
    return defaultValue;
}


bool Environment::getBool(const char* envConfigName, const bool defaultValue)
{
    using namespace std::string_view_literals;
    static std::set trueValues = {"1"sv, "true"sv, "yes"sv};
    static std::set falseValues = {"0"sv, "false"sv, "no"sv};
    const auto environmentVariable = Environment::get(envConfigName);

    if (environmentVariable.has_value())
    {
        auto val = String::toLower(environmentVariable.value());
        if (trueValues.count(val) > 0)
        {
            return true;
        }
        else if (falseValues.count(val) > 0)
        {
            return false;
        }
        TLOG(WARNING) << "Invalid Value in environment variable '"
                << *environmentVariable
                << "': '" << environmentVariable.value()
                << "' - expected boolean. using default: "
                << std::boolalpha << defaultValue;
    }
    return defaultValue;
}
