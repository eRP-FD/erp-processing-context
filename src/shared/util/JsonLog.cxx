/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/model/NumberAsStringParserWriter.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/JsonLog.hxx"
#include "shared/util/String.hxx"
#include "shared/util/TLog.hxx"

#include <iomanip>

JsonLog::JsonLog(const LogId id, std::ostream& os, bool withDetails)
    : JsonLog(
          id,
          [&os](const std::string& message) {
              os << message;
          },
          withDetails)
{
}


JsonLog::JsonLog(const LogId, LogReceiver&& receiver, bool withDetails)
    : mLogReceiver(std::move(receiver))
    , mShowDetails(withDetails)
{
    mDocument.SetObject();
}


JsonLog::~JsonLog()
{
    try
    {
        finish();
    }
    catch (...)
    {
        TLOG(ERROR) << "Exception in JsonLog destructor";
    }
}


JsonLog::LogReceiver JsonLog::makeErrorLogReceiver()
{
    return [](const std::string& message) {
        TLOG(ERROR) << message;
    };
}


JsonLog::LogReceiver JsonLog::makeWarningLogReceiver()
{
    return [](const std::string& message) {
        TLOG(WARNING) << message;
    };
}


JsonLog::LogReceiver JsonLog::makeInfoLogReceiver()
{
    return [](const std::string& message) {
        TLOG(INFO) << message;
    };
}


JsonLog::LogReceiver JsonLog::makeVLogReceiver(const int32_t logLevel)
{
    return [logLevel](const std::string& message) {
        TVLOG(logLevel) << message;
    };
}

void JsonLog::finish()
{
    LogReceiver receiver;
    receiver.swap(mLogReceiver);

    if (! receiver)
    {
        return;
    }

    if (mShowDetails)
    {
        mDocument.AddMember("details", mDetails, mDocument.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer(buffer);
    mDocument.Accept(writer);
    receiver(buffer.GetString());
}


JsonLog& JsonLog::message(const std::string_view text)
{
    if (mLogReceiver)
    {
        mDocument.AddMember(
            "info",
            rapidjson::Value{text.data(), gsl::narrow<rapidjson::SizeType>(text.size()), mDocument.GetAllocator()},
            mDocument.GetAllocator());
    }
    return *this;
}

JsonLog& JsonLog::locationFromException(const boost::exception& ex) &
{
    locationFromException<>(ex);
    return *this;
}

JsonLog&& JsonLog::locationFromException(const boost::exception& ex) &&
{
    locationFromException<>(ex);
    return std::move(*this);
}

JsonLog& JsonLog::locationFromException(const std::exception& ex) &
{
    locationFromException<>(ex);
    return *this;
}

JsonLog&& JsonLog::locationFromException(const std::exception& ex) &&
{
    locationFromException<>(ex);
    return std::move(*this);
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
            {
                mDocument.AddMember(
                    "rootLocation",
                    rapidjson::Value{to_string(optionalLocation->rootLocation), mDocument.GetAllocator()},
                    mDocument.GetAllocator());
            }
            mDocument.AddMember("location",
                                rapidjson::Value{to_string(optionalLocation->location), mDocument.GetAllocator()},
                                mDocument.GetAllocator());
        }
        else
        {
            mDocument.AddMember("location", "unknown", mDocument.GetAllocator());
        }
    }
}

JsonLog& JsonLog::location(const FileNameAndLineNumber& loc)
{
    if (mLogReceiver)
    {
        mDocument.AddMember("location", rapidjson::Value{to_string(loc), mDocument.GetAllocator()},
                            mDocument.GetAllocator());
    }
    return *this;
}


JsonLog& JsonLog::details(const std::string_view details)
{
    mDetails = details;
    return *this;
}


JsonLog& JsonLog::keyValue(const std::string_view key, const std::string_view value)
{
    if (mLogReceiver)
    {
        mDocument.AddMember(
            rapidjson::Value{key.data(), gsl::narrow<rapidjson::SizeType>(key.size()), mDocument.GetAllocator()},
            rapidjson::Value{value.data(), gsl::narrow<rapidjson::SizeType>(value.size()), mDocument.GetAllocator()},
            mDocument.GetAllocator());
    }
    return *this;
}


JsonLog& JsonLog::keyValue(const std::string_view key, const size_t value)
{
    if (mLogReceiver)
    {
        mDocument.AddMember(
            rapidjson::Value{key.data(), gsl::narrow<rapidjson::SizeType>(key.size()), mDocument.GetAllocator()},
            rapidjson::Value{value}, mDocument.GetAllocator());
    }
    return *this;
}


JsonLog& JsonLog::keyValue(const std::string_view key, const uint16_t value)
{
    if (mLogReceiver)
    {
        mDocument.AddMember(
            rapidjson::Value{key.data(), gsl::narrow<rapidjson::SizeType>(key.size()), mDocument.GetAllocator()},
            rapidjson::Value{value}, mDocument.GetAllocator());
    }
    return *this;
}


JsonLog& JsonLog::keyValue(const std::string_view key, const double value)
{
    if (mLogReceiver)
    {
        mDocument.AddMember(
            rapidjson::Value{key.data(), gsl::narrow<rapidjson::SizeType>(key.size()), mDocument.GetAllocator()},
            rapidjson::Value{value}, mDocument.GetAllocator());
    }
    return *this;
}


void JsonLog::discard()
{
    mLogReceiver = {};
}

JsonLog& operator<<(JsonLog& jsonLog, const KeyValue& kv)
{
    jsonLog.keyValue(kv.key, kv.value);
    return jsonLog;
}

JsonLog&& operator<<(JsonLog&& jsonLog, const KeyValue& kv)
{
    jsonLog.keyValue(kv.key, kv.value);
    return std::move(jsonLog);
}
