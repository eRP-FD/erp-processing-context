/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/log/JsonLogImplementation.hxx"
#include "library/log/Logging.hxx"
#include "library/log/PlainLogImplementation.hxx"
#include "library/util/Configuration.hxx"

#include <map>
#include <thread>

#include "shared/util/String.hxx"

namespace epa
{

Log::LogStream::LogStream(Log::LogStream::UserData& userData)
  : mUserData(&userData)
{
}


Log::LogStream::UserData& Log::LogStream::getUserData()
{
    return *mUserData;
}


void Log::LogStream::setUserData(Log::LogStream::UserData& userData)
{
    mUserData = &userData;
}


Log::LogStream::UserData* Log::LogStream::getUserData(std::ostream& os)
{
    auto* logStream = dynamic_cast<LogStream*>(&os);
    if (logStream == nullptr)
        return nullptr;
    else
        return &logStream->getUserData();
}


void Log::LogStream::addClassifiedText(const ClassifiedText& classifiedText)
{
    // This is a special solution that allows to maintain
    // one message string with replacement for log
    // and one message string replacement text for responses
    // at the point where they might differ (ClassifiedText).

    // Append the stream content that has accumulated since
    // the last ClassifiedText (or since construction).
    mLogText += std::stringstream::str();
    mResponseText += std::stringstream::str();

    // Reset the base class data.
    std::stringstream::str(std::string());
    clear();

    // replace the classified text with, in general, different replacements for log and response
    mLogText += classifiedText.toString(LogContext::Log);
    mResponseText += classifiedText.toString(LogContext::Response);
}


std::string Log::LogStream::str() const
{
    return getLogText();
}


std::string Log::LogStream::getLogText() const
{
    // get the log-specific part of the text concatenated with the normal text in stream
    return mLogText + std::stringstream::str();
}


std::string Log::LogStream::getResponseText() const
{
    // get the response-specific part of the text concatenated with the normal text in stream
    return mResponseText + std::stringstream::str();
}


/**
 * This flag is used to select between JSON and text format for log messages.
 * Note that the value is not fixed so that tests can switch between the two formats.
 * Note also that we can not use `Configuration` to access it because that class depends on the
 * LOG framework already being initialized. That is not the case at the time of initialization
 * of static variables.
 */
EnvironmentValue<bool> Log::logFormatIsJson(
    config_constants::CONFIG_LOG_FORMAT_ENV,
    DEFAULT_LOG_FORMAT_ENV,
    [](auto value) { return String::toUpper(std::string(value)) == "JSON"; });

EnvironmentValue<int> Log::logLevel(
    config_constants::CONFIG_LOG_LEVEL_ENV,
    DEFAULT_LOG_LEVEL_ENV,
    [](auto value) { return parseLogLevel(value); });


Log::Log(const Severity severity, const Location& location, const Mode mode)
  : mImplementation(
        logFormatIsJson.getValue()
            ? std::make_unique<JsonLogImplementation>(severity, location, mode)
            : std::make_unique<PlainLogImplementation>(severity, location, mode))
{
}


Log::~Log()
{
    mImplementation->outputLog(true);
}


Log::LogStream& Log::stream()
{
    return mImplementation->stream();
}


std::string Log::getMessage(LogContext logContext) const
{
    return mImplementation->getMessage(logContext);
}


void Log::outputLog(bool allowTraceForImmediate)
{
    mImplementation->outputLog(allowTraceForImmediate);
}


Log::Implementation& Log::getImplementation()
{
    return *mImplementation;
}


bool Log::isJson() const
{
    return mImplementation->isJson();
}


void Log::setFlag(const Flag flag)
{
    mImplementation->setFlag(flag);
}


std::ostream& operator<<(std::ostream& os, const Log::KeyValue& keyValue)
{
    auto* handler = dynamic_cast<Log::IKeyValueHandler*>(Log::LogStream::getUserData(os));
    if (handler != nullptr)
        handler->addKeyValue(keyValue);
    return os;
}


std::ostream& operator<<(std::ostream& os, const Log::JsonSerializer& jsonSerializer)
{
    auto* handler = dynamic_cast<Log::IKeyValueHandler*>(Log::LogStream::getUserData(os));
    if (handler != nullptr)
        handler->addJsonSerializer(jsonSerializer);
    return os;
}


std::ostream& operator<<(std::ostream& os, const Location& location)
{
    auto* handler = dynamic_cast<Log::ILocationConsumer*>(Log::LogStream::getUserData(os));
    if (handler != nullptr)
        handler->setLocation(location);
    return os;
}


std::ostream& operator<<(std::ostream& os, const Log::Flag flag)
{
    auto* handler = dynamic_cast<Log::IFlagConsumer*>(Log::LogStream::getUserData(os));
    if (handler != nullptr)
        handler->setFlag(flag);
    return os;
}


namespace logging
{
    Log::KeyValue keyValue(const std::string& key, const std::string& value)
    {
        return Log::KeyValue{key, value};
    }

    Log::KeyValue keyValue(const std::string& key, const uint64_t value)
    {
        return Log::KeyValue{key, value};
    }

    Log::KeyValue keyValue(const std::string& key, const int64_t value)
    {
        return Log::KeyValue{key, value};
    }

    Log::KeyValue keyValue(const std::string& key, const double value)
    {
        return Log::KeyValue{key, value};
    }

    Log::Flag withFlag(Log::Flag flag)
    {
        return flag;
    }

    Log::JsonSerializer jsonWriter(Log::JsonSerializer::CallbackType&& callback)
    {
        return Log::JsonSerializer{std::move(callback)};
    }


    /**
     * Note that the signatures of the following factory methods for classified text is not final.
     * Depending on the majority of use cases the `text` argument might be passed as const reference
     * or string_view. Maybe different overloads will be provided.
     */

    ClassifiedText personal(std::string&& text, std::string replacement)
    {
        return ClassifiedText(
            std::move(text), std::move(replacement), ClassifiedText::Visibility::ShowInResponse);
    }


    ClassifiedText sensitive(std::string&& text, std::string replacement)
    {
        return ClassifiedText(
            std::move(text), std::move(replacement), ClassifiedText::Visibility::ShowInResponse);
    }


    ClassifiedText confidential(std::string&& text, std::string replacement)
    {
        return ClassifiedText(
            std::move(text), std::move(replacement), ClassifiedText::Visibility::ShowInLog);
    }


    ClassifiedText development(std::string&& text, std::string replacement)
    {
        return ClassifiedText(
            std::move(text),
            std::move(replacement),
            ClassifiedText::Visibility::ShowInDevelopmentBuild);
    }
}


int Log::parseLogLevel(std::string_view rawValue)
{
    static const std::map<std::string, int> stringToIntMap = {
        {"3", 3},
        {"2", 2},
        {"1", 1},
        {"0", 0},
        {"-1", -1},
        {"-2", -2},
        {"-3", -3},
        {"-4", -4},
        {"FATAL", 3},
        {"ERROR", 2},
        {"WARNING", 1},
        {"INFO", 0},
        {"INFO0", 0},
        {"INFO1", -1},
        {"INFO2", -2},
        {"INFO3", -3},
        {"INFO4", -4},
        {"DEBUG", 0},
        {"DEBUG0", 0},
        {"DEBUG1", -1},
        {"DEBUG2", -2},
        {"DEBUG3", -3},
        {"DEBUG4", -4},

        // Allow some frequently made abbreviations
        {"ERR", 2},
        {"WARN", 1}};
    const std::string value = String::toUpper(String::trim(std::string{rawValue}));
    auto iterator = stringToIntMap.find(value);
    if (iterator != stringToIntMap.end())
        return iterator->second;
    iterator = stringToIntMap.find(DEFAULT_LOG_LEVEL_ENV);
    if (iterator != stringToIntMap.end())
        return iterator->second;
    return 0;
}

} // namespace epa
