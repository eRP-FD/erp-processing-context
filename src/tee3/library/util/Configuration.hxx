/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_CONFIGURATION_HXX
#define EPA_LIBRARY_UTIL_CONFIGURATION_HXX

#include "library/wrappers/RapidJson.hxx"

#include <string>

#include "shared/util/Configuration.hxx"

namespace epa
{

namespace config_constants
{

    // Logging
    constexpr static const char* CONFIG_LOG_FORMAT_ENV = "LOG_FORMAT";
    // No JSON path for this variable.
    constexpr static const char* DEFAULT_LOG_FORMAT_ENV = "text";
    constexpr static const char* CONFIG_LOG_LEVEL_ENV = "LOG_LEVEL";
    // No JSON path for this variable.
    constexpr static const char* DEFAULT_LOG_LEVEL_ENV = "INFO2"; // the same as "-2"
    constexpr static const char* CONFIG_TRACE_LOG_MAX_COUNT = "TRACE_LOG_MAX_COUNT";
    constexpr static const char* CONFIG_JSON_TRACE_LOG_MAX_COUNT = "/config/log/trace/maxCount";
    constexpr int DEFAULT_TRACE_LOG_MAX_COUNT = 128;
    constexpr static const char* CONFIG_TRACE_LOG_MODE = "TRACE_LOG_MODE";
    // constexpr static const char* CONFIG_JSON_TRACE_LOG_MODE = "/config/log/trace/mode";
    constexpr static const char* DEFAULT_TRACE_LOG_MODE = "DEFERRED";
}

using config_constants::DEFAULT_LOG_FORMAT_ENV;
using config_constants::DEFAULT_LOG_LEVEL_ENV;
using config_constants::DEFAULT_TRACE_LOG_MAX_COUNT;
using config_constants::DEFAULT_TRACE_LOG_MODE;

enum ConfigurationKey
{
    CONFIG_LOG_FORMAT_ENV,
    CONFIG_LOG_LEVEL_ENV,
    CONFIG_TRACE_LOG_MAX_COUNT,
    CONFIG_JSON_TRACE_LOG_MAX_COUNT,
    CONFIG_TRACE_LOG_MODE,
    CONFIG_JSON_TRACE_LOG_MODE,
};

using KeyNames = ConfigurationKeyNamesBase<ConfigurationKey>;


/**
 * A simple helper class to access the common configuration that can be provided both as
 * environment variables as well as a JSON configuration file. The configuration file have to
 * be in the current working directory
 *
 */
class Configuration : public ::ConfigurationTemplate<ConfigurationKey, KeyNames>
{
public:
    /**
     * @return the singleton instance, it is created the first time the method is called
     */
    static const Configuration& getInstance();
};

} // namespace epa

#endif
