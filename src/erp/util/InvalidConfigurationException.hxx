/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_UTIL_INVALIDCONFIGURATIONEXCEPTION_HXX
#define E_LIBRARY_UTIL_INVALIDCONFIGURATIONEXCEPTION_HXX

#include <stdexcept>

/**
 * Use this class to express configuration errors, i.e. missing but mandatory configuration values.
 */
class InvalidConfigurationException
    : public std::runtime_error
{
public:
    /**
     * The constructor expects both the name of an environment variable and the path to the corresponding
     * value in a JSON configuration file.  These names are used only for display, they are not interpreted or
     * used to access the associated values.
     */
    InvalidConfigurationException (const std::string& message,
        const std::string& environmentVariableName, const std::string& jsonConfigPath);
};

#endif
