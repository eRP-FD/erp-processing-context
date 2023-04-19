/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/JsonLog.hxx"

#include "erp/util/Configuration.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/String.hxx"

JsonLog::JsonLog (const LogId id, std::ostream& os, bool withDetails)
    : JsonLog(id, [&os](const std::string& message){os<<message;}, withDetails)
{
}


JsonLog::JsonLog (const LogId id, LogReceiver&& receiver, bool withDetails)
    : mLogReceiver(std::move(receiver)),
      mShowDetails(withDetails),
      mLog(),
      mDetails()
{
    mLog << "{\"id\":" << static_cast<uint32_t>(id);

    const auto& configuration = Configuration::instance();
    keyValue("host", configuration.serverHost());
    keyValue("port", configuration.serverPort());
}


JsonLog::~JsonLog (void)
{
    finish();
}


JsonLog::LogReceiver JsonLog::makeErrorLogReceiver()
{
    return [](const std::string& message)
    {
        TLOG(ERROR) << message;
    };
}


JsonLog::LogReceiver JsonLog::makeWarningLogReceiver()
{
    return [](const std::string& message)
    {
        TLOG(WARNING) << message;
    };
}


JsonLog::LogReceiver JsonLog::makeInfoLogReceiver()
{
    return [](const std::string& message)
    {
        TLOG(INFO) << message;
    };
}


JsonLog::LogReceiver JsonLog::makeVLogReceiver (const int32_t logLevel)
{
    return [logLevel](const std::string& message)
    {
        TVLOG(logLevel) << message;
    };
}


void JsonLog::finish (void)
{
    LogReceiver receiver;
    receiver.swap(mLogReceiver);

    if ( ! receiver)
        return;

    if (mShowDetails)
        mLog << R"(,"details":")" << escapeJson(mDetails) << R"(")";
    mLog << '}';

    receiver(mLog.str());
}


JsonLog& JsonLog::message (const std::string_view text)
{
    if (mLogReceiver)
        mLog << R"(,"info":")" << escapeJson(std::string(text)) << R"(")";
    return *this;
}

void JsonLog::locationFromException(const boost::exception& ex)
{
    locationFromException<>(ex);
}

void JsonLog::locationFromException(const std::exception& ex)
{
    locationFromException<>(ex);
}

template<typename ExceptionT>
void JsonLog::locationFromException(const ExceptionT& ex)
{
    if (mLogReceiver)
    {
        auto optionalLocation = ExceptionWrapper<ExceptionT>::getLocation(ex);
        if (optionalLocation.has_value())
        {
            if (optionalLocation->location != optionalLocation->rootLocation)
                mLog << R"(, "rootLocation": ")" << optionalLocation->rootLocation << '\"';
            else
                mLog << R"(, "location": ")" << optionalLocation->location << '\"';
        }
        else
        {
            mLog << R"(, "location": "unknown")";
        }
    }
}

JsonLog& JsonLog::location(const FileNameAndLineNumber& loc)
{
    if (mLogReceiver)
        mLog << R"(,"location": ")" << loc << '\"';
    return *this;
}


JsonLog& JsonLog::details (const std::string_view details)
{
    mDetails = details;
    return *this;
}


JsonLog& JsonLog::keyValue (const std::string_view key, const std::string_view value)
{
    if (mLogReceiver)
        mLog << ",\"" << key << "\":\"" << escapeJson(std::string(value)) << '\"';
    return *this;
}


JsonLog& JsonLog::keyValue (const std::string_view key, const size_t value)
{
    if (mLogReceiver)
        mLog << ",\"" << key << "\":" << value;
    return *this;
}


JsonLog& JsonLog::keyValue (const std::string_view key, const uint16_t value)
{
    if (mLogReceiver)
        mLog << ",\"" << key << "\":" << value;
    return *this;
}


JsonLog& JsonLog::keyValue (const std::string_view key, const double value, const uint8_t precision)
{
    if (mLogReceiver)
    {
        mLog << ",\"" << key << "\":";
        if (precision > 0)
        {
            mLog << std::fixed << std::setprecision(precision);
        }
        mLog << value;
        if (precision > 0)
            mLog << std::scientific;
    }
    return *this;
}


void JsonLog::discard (void)
{
    mLogReceiver = {};
}


std::string JsonLog::escapeJson (const std::string& text)
{
    return String::replaceAll(text, "\"", "\\\"");
}
