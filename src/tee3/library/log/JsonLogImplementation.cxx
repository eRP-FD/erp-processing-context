/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/log/JsonLogImplementation.hxx"
#include "library/log/Logging.hxx"
#include "library/log/SessionId.hxx"
#include "library/util/Time.hxx"
#include "library/wrappers/RapidJson.hxx"

#include <date/date.h>

#include "shared/util/ThreadNames.hxx"

namespace epa
{

namespace
{

    void writeKeyValue(
        rapidjson::Writer<rapidjson::StringBuffer>& writer,
        const std::string_view key,
        const std::string_view value)
    {
        writer.Key(key.begin(), gsl::narrow<rapidjson::SizeType>(key.size()));
        writer.String(value.begin(), gsl::narrow<rapidjson::SizeType>(value.size()));
    }

    void writeKeyValue(
        rapidjson::Writer<rapidjson::StringBuffer>& writer,
        const std::string_view key,
        const int64_t value)
    {
        writer.Key(key.begin(), gsl::narrow<rapidjson::SizeType>(key.size()));
        writer.Int64(value);
    }

    void writeKeyValue(
        rapidjson::Writer<rapidjson::StringBuffer>& writer,
        const std::string_view key,
        const uint64_t value)
    {
        writer.Key(key.begin(), gsl::narrow<rapidjson::SizeType>(key.size()));
        writer.Uint64(value);
    }

    void writeKeyValue(
        rapidjson::Writer<rapidjson::StringBuffer>& writer,
        const std::string_view key,
        const double value)
    {
        writer.Key(key.begin(), gsl::narrow<rapidjson::SizeType>(key.size()));
        writer.Double(value);
    }

    SystemTime toSystemClock(const PlainLogImplementation::ClockType::time_point time)
    {
        // Strangely the expression `std::is_same_v<std::chrono::steady_clock, std::chrono::system_clock>`
        // always returns true. That does not allow a compile-time check of whether ClockType is the system_clock
        // or the steady_clock. Therefore we always have to convert.

        // https://stackoverflow.com/questions/35282308/convert-between-c11-clocks
        // suggests that the following is the best we can do.
        const auto sourceNow = PlainLogImplementation::ClockType::now();
        const auto destinationNow = SystemTime::clock::now();
        return destinationNow
               + std::chrono::duration_cast<std::chrono::microseconds>(time - sourceNow);
    }
}


JsonLogImplementation::JsonLogImplementation(
    const Log::Severity severity,
    const Location& location,
    const Log::Mode mode)
  : PlainLogImplementation(severity, location, mode),
    mKeyPairs(),
    mJsonSerializerCallbacks(),
    mShowSessionDescription(false)
{
}


std::string JsonLogImplementation::assembleLogLine() const
{
    try
    {
        rapidjson::StringBuffer jsonBuffer;
        rapidjson::Writer<rapidjson::StringBuffer> jsonWriter(jsonBuffer);
        jsonWriter.StartObject();

        // Preprocess some values.
        const auto& sessionId = SessionId::current();
        const SystemTime time = toSystemClock(mTime);

        writeKeyValue(jsonWriter, "severity", Log::getSeverityString(mSeverity));
        writeKeyValue(jsonWriter, "message", getMessage(LogContext::Log));
        writeKeyValue(jsonWriter, "thread", ThreadNames::instance().getCurrentThreadName());
        writeKeyValue(
            jsonWriter, "utc", date::format(std::locale::classic(), "%Y-%m-%d %H:%M:%S", time));
        writeKeyValue(jsonWriter, "location", getLocation().toShortString());

        // Write session information.
        jsonWriter.Key("session");
        jsonWriter.StartObject();
        writeKeyValue(jsonWriter, "id", sessionId.sessionId());
        writeKeyValue(jsonWriter, "internal", sessionId.uniqueSessionId());
        writeKeyValue(jsonWriter, "traceId", sessionId.traceId());
        if (mShowSessionDescription)
            writeKeyValue(jsonWriter, "description", sessionId.description());
        jsonWriter.EndObject();

        for (const auto& keyPair : mKeyPairs)
        {
            if (std::holds_alternative<std::string>(keyPair.value))
                writeKeyValue(jsonWriter, keyPair.key, std::get<std::string>(keyPair.value));
            else if (std::holds_alternative<int64_t>(keyPair.value))
                writeKeyValue(jsonWriter, keyPair.key, std::get<int64_t>(keyPair.value));
            else if (std::holds_alternative<uint64_t>(keyPair.value))
                writeKeyValue(jsonWriter, keyPair.key, std::get<uint64_t>(keyPair.value));
            else if (std::holds_alternative<double>(keyPair.value))
                writeKeyValue(jsonWriter, keyPair.key, std::get<double>(keyPair.value));
            else
                Failure() << "unsupported JSON type with type index " << keyPair.value.index();
        }

        for (const auto& jsonWriterCallback : mJsonSerializerCallbacks)
        {
            jsonWriterCallback(jsonWriter);
        }


        // Finish JSON writing.
        jsonWriter.EndObject();
        jsonWriter.Flush();

        return std::string(jsonBuffer.GetString(), jsonBuffer.GetLength());
    }
    catch (const std::exception& e)
    {
        // Don't let an exception escape and cause the application to crash (undeclared exception
        // from destructor). Print it and then ignore it.
        return "caught exception while creating log message: " + std::string(e.what());
    }
}


void JsonLogImplementation::addKeyValue(const Log::KeyValue& keyValue)
{
    mKeyPairs.emplace_back(keyValue);
}


void JsonLogImplementation::addJsonSerializer(const Log::JsonSerializer& serializer)
{
    mJsonSerializerCallbacks.emplace_back(serializer.callback);
}


bool JsonLogImplementation::isJson() const
{
    return true;
}


void JsonLogImplementation::setFlag(const Log::Flag flag)
{
    switch (flag)
    {
        case Log::Flag::ShowSessionDescription:
            mShowSessionDescription = true;
            break;
    }
}

} // namespace epa
