/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/InvalidConfigurationException.hxx"

#include <sstream>

namespace {
    std::string makeMessage (const std::string& message,
        const std::string& environmentVariableName, const std::string& jsonConfigPath)
    {
        std::stringstream s;
        s << message
          << ": env("
          << environmentVariableName
          << ") or json("
          << jsonConfigPath
          << ")";
        return s.str();
    }
}


InvalidConfigurationException::InvalidConfigurationException (
    const std::string& message,
    const std::string& environmentVariableName,
    const std::string& jsonConfigPath)
    : std::runtime_error(makeMessage(message, environmentVariableName, jsonConfigPath))
{
}
