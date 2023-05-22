/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/GLogConfiguration.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/Uuid.hxx"

#include <iomanip>

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


void GLogConfiguration::init_logging(const std::string_view& appName, const bool logToStderr,
                                     const bool stderrThreshold, const int minLogLevel, const std::string& logDir,
                                     const int vLogMaxValue)
{
    FLAGS_logtostderr = logToStderr;
    FLAGS_stderrthreshold = stderrThreshold;
    FLAGS_minloglevel = minLogLevel;
    FLAGS_log_dir = logDir;
    FLAGS_v = vLogMaxValue;

    google::InitGoogleLogging(appName.data(), &GLogConfiguration::erpLogPrefix, nullptr);

    TLOG(INFO) << "initialized logging: logtostderr=" << logToStderr << ", stderrthreshold=" << stderrThreshold
               << ", minLogLevel=" << minLogLevel << ", logDir=" << logDir << ", vLogMaxValue=" << vLogMaxValue;
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
        TLOG(WARNING) << "Invalid log level '" << logLevelString << "' defaulting to ERROR";
        // An unexpected value is provided. Fall back to "ERROR".
        return 2;
    }
}

void GLogConfiguration::erpLogPrefix(std::ostream& s, const google::LogMessageInfo& lmi, void*)
{
    using std::setfill;
    using std::setw;
    std::ostream oss(s.rdbuf());
    // clang-format off
    oss << setw(4) << 1900 + lmi.time.year() << '-'
        << setw(2) << setfill('0') << 1 + lmi.time.month() << '-'
        << setw(2) << lmi.time.day()
        << 'T'
        << setw(2) << lmi.time.hour() << ':'
        << setw(2) << lmi.time.min() << ':'
        << setw(2) << lmi.time.sec() << "."
        << setw(6) << lmi.time.usec()
        << ((lmi.time.gmtoff() < 0) ? '-' : '+')
        << setw(2) << lmi.time.gmtoff() / 3600 << ':'
        << setw(2) << (lmi.time.gmtoff() % 3600) / 60
        << ' '
        << setw(7) << setfill(' ') << std::left << lmi.severity
        << ' '
        << lmi.filename << ':' << lmi.line_number << "]";
    // clang-format on
}
