/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_GLOGCONFIGURATION_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_GLOGCONFIGURATION_HXX

#include <string_view>

namespace google
{
class LogMessage;
}
class GLogConfiguration {
public:
    /**
     * Initialized the glog with specified app name, the configuration is read from environment variables
     * The environment variables used to configure the logging
     * ERP_LOG_TO_STDERR - ignore log files, log to stderr
     * ERP_STDERR_THRESHOLD - duplicate log at and above level to stderr
     * ERP_MIN_LOG_LEVEL - minimal log level: INFO, WARNING, ERROR, FATAL
     * ERP_LOG_DIR - log directory
     * ERP_VLOG_MAX_VALUE - show the logs for the v-value and below
     * ERP_VLOG_CONFIG - (not yet activated)
     *                   Per-module verbose level. The argument has to contain a comma-separated
     *                   list of <module name>=<log level>. <module name> is a glob pattern
     *                   (e.g., gfs* for all modules whose name starts with "gfs"),
     *                   matched against the filename base (that is, name ignoring .cc/.h./-inl.h).
     *                   <log level> overrides any value given by ERP_VLOG_MAX_VALUE
     * @param appName - the name of the application
     */
    static void initLogging(const char* argv0);

    /**
     * Initializes the glog with specified configuration
     * @param appName - the name of the application
     * @param logToStderr - ignore log files, log to stderr
     * @param stderrThreshold - duplicate log at and above level to stderr
     * @param minLogLevel - minimal log level INFO == 0
     * @param logDir - log directory
     * @param vLogMaxValue - show the logs for the v-value and below
     */
    static void initLogging(
        const char* argv0,
        bool logToStderr,
        bool stderrThreshold,
        int minLogLevel,
        const std::string& logDir,
        int vLogMaxValue);

private:
    static int getLogLevelInt(const std::string& logLevelString);
    static void erpLogPrefix(std::ostream& ostream, const google::LogMessage& lmi, void*);
};

#endif
