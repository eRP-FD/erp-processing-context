/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/GLogConfiguration.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/TLog.hxx"

#include <iomanip>

void GLogConfiguration::initLogging(const char* argv0)
{
    const bool logToStderr = Environment::getBool("ERP_LOG_TO_STDERR", false);
    const int stderrThreshold = Environment::getInt("ERP_STDERR_THRESHOLD", 0);
    const std::string minLogLevelString = Environment::getString("ERP_MIN_LOG_LEVEL", "INFO");
    const std::string logDir = Environment::getString("ERP_LOG_DIR", "/tmp");
    const int vLogMaxValue = Environment::getInt("ERP_VLOG_MAX_VALUE", 1);

    initLogging(argv0, logToStderr, stderrThreshold > 0, getLogLevelInt(minLogLevelString), logDir, vLogMaxValue);
}


void GLogConfiguration::initLogging(const char* argv0, const bool logToStderr,
                                     const bool stderrThreshold, const int minLogLevel, const std::string& logDir,
                                     const int vLogMaxValue)
{
    FLAGS_logtostderr = logToStderr;
    FLAGS_stderrthreshold = stderrThreshold;
    FLAGS_minloglevel = minLogLevel;
    FLAGS_log_dir = logDir;
    FLAGS_v = vLogMaxValue;

    google::InitGoogleLogging(argv0);
    google::InstallPrefixFormatter(&GLogConfiguration::erpLogPrefix, nullptr);

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

void GLogConfiguration::erpLogPrefix(std::ostream& ostream, const google::LogMessage& lmi, void*)
{
    using std::setfill;
    using std::setw;
    std::ostream oss(ostream.rdbuf());
    // clang-format off
    oss << setw(4) << 1900 + lmi.time().year() << '-'
        << setw(2) << setfill('0') << 1 + lmi.time().month() << '-'
        << setw(2) << lmi.time().day()
        << 'T'
        << setw(2) << lmi.time().hour() << ':'
        << setw(2) << lmi.time().min() << ':'
        << setw(2) << lmi.time().sec() << "."
        << setw(6) << lmi.time().usec()
        << ((lmi.time().gmtoff() < 0) ? '-' : '+')
        << setw(2) << lmi.time().gmtoff() / 3600 << ':'
        << setw(2) << (lmi.time().gmtoff() % 3600) / 60 << ' '
        << setw(7) << setfill(' ') << std::left
        << google::GetLogSeverityName(lmi.severity())  << ' '
        << lmi.basename() << ':' << lmi.line() << "]";
    // clang-format on
}
