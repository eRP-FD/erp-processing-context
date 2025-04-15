/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_LOG_ENVIRONMENTVALUE_HXX
#define EPA_LIBRARY_LOG_ENVIRONMENTVALUE_HXX

#include <functional>
#include <string_view>
#include <cstdlib>

namespace epa
{

/**
 * Provide easy access to the value of an environment variable.
 * The string of the environment variable is converted into the actual value with a conversion
 * lambda.
 * The value remains mutable so that e.g. tests can manipulate the value.
 */
template<typename T>
class EnvironmentValue
{
public:
    EnvironmentValue(
        const char* variableName,
        const char* defaultValue,
        std::function<T(std::string_view)>&& stringToTConverter);

    const T& getValue();

protected:
    void setValue(const T& value);

private:
    T mValue;

    /**
     * Evaluate the current value of the environment variable with name `name`.
     * If set, apply `stringToTConverter` and return the result.
     * If not set, return `defaultValue`.
     */
    static T evaluate(
        const char* name,
        const char* defaultValue,
        const std::function<T(std::string_view)>& stringToTConverter);
};


template<typename T>
EnvironmentValue<T>::EnvironmentValue(
    const char* variableName,
    const char* defaultValue,
    std::function<T(std::string_view)>&& stringToTConverter)
  : mValue(evaluate(variableName, defaultValue, std::move(stringToTConverter)))
{
}


template<typename T>
const T& EnvironmentValue<T>::getValue()
{
    return mValue;
}


template<typename T>
void EnvironmentValue<T>::setValue(const T& value)
{
    mValue = value;
}


template<typename T>
T EnvironmentValue<T>::evaluate(
    const char* name,
    const char* defaultValue,
    const std::function<T(std::string_view)>& stringToTConverter)
{
    // We can not use class `Configuration` to get the value of the environment value because `Configuration`
    // writes log messages. That could lead to a deadlock caused by non-reentrant mutex.
    // Therefore, we use std::getenv, the slightly more thread-safe version of getenv. getenv_s
    // would be even better but that does not seem to be available.
    //NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char* stringValue = std::getenv(name);
    return stringToTConverter(
        std::string_view(stringValue != nullptr ? stringValue : defaultValue));
}
} // namespace epa

#endif
