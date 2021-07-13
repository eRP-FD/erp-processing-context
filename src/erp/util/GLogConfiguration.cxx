#include "erp/util/GLogConfiguration.hxx"

#include "erp/util/Environment.hxx"
#include "erp/util/Uuid.hxx"

void GLogConfiguration::init_logging(const std::string_view& appName)
{
    const bool logToStderr = Environment::getBool("ERP_LOG_TO_STDERR", false);
    const int stderrThreshold = Environment::getInt("ERP_STDERR_THRESHOLD", 0);
    const std::string minLogLevelString = Environment::getString("ERP_MIN_LOG_LEVEL", "INFO");
    const std::string logDir = Environment::getString("ERP_LOG_DIR", "/tmp");
    const int vLogMaxValue = Environment::getInt("ERP_VLOG_MAX_VALUE", 1);
    const std::string vLogConfig = Environment::getString("ERP_VLOG_CONFIG", "");

    init_logging(appName, logToStderr, stderrThreshold, getLogLevelInt(minLogLevelString), logDir, vLogMaxValue);
}


void GLogConfiguration::init_logging(
    const std::string_view& appName,
    const bool logToStderr,
    const bool stderrThreshold,
    const int minLogLevel,
    const std::string& logDir,
    const int vLogMaxValue)
{
    FLAGS_logtostderr = logToStderr;
    FLAGS_stderrthreshold = stderrThreshold;
    FLAGS_minloglevel = minLogLevel;
    FLAGS_log_dir = logDir;
    FLAGS_v = vLogMaxValue;

    google::InitGoogleLogging(appName.data());

    LOG(INFO) << "initialized logging: logtostderr=" << logToStderr
              << ", stderrthreshold=" << stderrThreshold
              << ", minLogLevel=" << minLogLevel
              << ", logDir=" << logDir
              << ", vLogMaxValue=" << vLogMaxValue;
}

int GLogConfiguration::getLogLevelInt(const std::string& logLevelString)
{
    if (logLevelString == "INFO")
        return 0;
    else if (logLevelString == "WARNING")
        return 1;
    else if (logLevelString == "ERROR")
        return 2;
    else if (logLevelString == "FATAL")
        return 3;
    else
    {
        LOG(WARNING) << "Invalid log level '" << logLevelString << "' defaulting to ERROR";
        // An unexpected value is provided. Fall back to "ERROR".
        return 2;
    }
}


std::string GLogConfiguration::getProcessUuid()
{
    // it is the most compatible way for different OSs just to generate the own Uuid for the process
    static ::std::string processUuid = Uuid().toString();

    return processUuid;
}
