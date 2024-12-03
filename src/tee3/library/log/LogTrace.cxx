/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/log/LogTrace.hxx"
#include "library/util/Configuration.hxx"

#include <glog/raw_logging.h>

#include "shared/util/String.hxx"

namespace epa
{

namespace
{
    size_t getConfiguredMaxMessageCount()
    {
        return gsl::narrow<size_t>(Configuration::getInstance().getOptionalIntValue(
            CONFIG_TRACE_LOG_MAX_COUNT, DEFAULT_TRACE_LOG_MAX_COUNT));
    }
    size_t getMaxMessageCount()
    {
        static const size_t maxMessageCount = getConfiguredMaxMessageCount();
        return maxMessageCount;
    }
    std::string getCutOffString(const size_t cutOffCount)
    {
        switch (cutOffCount)
        {
            case 0:
                return "";
            case 1:
                return ", the first was cut off";
            default:
                return ", the first " + std::to_string(cutOffCount) + " where cut off";
        }
    }

    /**
     * Prefix the message with information about the position inside the trace.
     * Using string replacement for this is rather crude but at the time a log message is
     * handed over to LogTrace, its JSON content has already been serialized to a string.
     * Adding another callback or something similar into the construction process of a log message
     * and with that gain access to raw, unserialized message would be possible but more complex
     * than this simple solution.
     */
    void prefixTraceMessage(
        std::string& message,
        const size_t index,
        const std::string& countString)
    {
        constexpr static std::string_view intro = R"("message":")";

        const auto introStart = message.find(intro);
        if (introStart != std::string::npos)
        {
            message.insert(
                introStart + intro.size(),
                "    trace " + std::to_string(index) + "/" + countString + ": ");
        }
    }
}


// NOLINTNEXTLINE(misc-no-recursion)
LogTraceMode::Mode LogTraceMode::fromString(
    const std::string_view value,
    const std::string_view defaultMode)
{
    const auto upperValue = String::toUpper(std::string(value));
    if (upperValue == "DEFERRED")
        return Mode::Deferred;
    else if (upperValue == "IGNORED")
        return Mode::Ignored;
    else if (upperValue == "IMMEDIATE")
        return Mode::Immediate;
    else
    {
        // Prevent infinite recursion.
        Assert(! defaultMode.empty()) << "invalid default log trace mode";

        google::LogMessage(__FILE__, __LINE__, google::GLOG_WARNING).stream()
            << "invalid log trace mode value '" << std::string(value) << "', using default '"
            << defaultMode << "'";
        return fromString(defaultMode, {});
    }
}


std::string LogTraceMode::toString(LogTraceMode::Mode value)
{
    if (value == Mode::Deferred)
        return "Deferred";
    else if (value == Mode::Ignored)
        return "Ignored";
    else if (value == Mode::Immediate)
        return "Immediate";
    else
        Failure() << "unknown LogTraceMode::Mode value";
}


EnvironmentValue<LogTraceMode::Mode> LogTrace::currentLogMode(
    config_constants::CONFIG_TRACE_LOG_MODE,
    DEFAULT_TRACE_LOG_MODE,
    [](auto value) { return LogTraceMode::fromString(value, DEFAULT_TRACE_LOG_MODE); });


LogTrace::LogTrace()
{
    if (! mThreadData.logTraceInstanceCount)
        mThreadData.container = std::make_unique<Container>();
    mThreadData.logTraceInstanceCount++;
}


LogTrace::~LogTrace()
{
    mThreadData.logTraceInstanceCount--;
    if (! mThreadData.logTraceInstanceCount)
        mThreadData.container.reset();
}


void LogTrace::prepareMessage(Log::LogStream& stream, const Log::Mode mode)
{
    if (mThreadData.container)
    {
        // Mark the message as "deferred" or "immediate".
        if (mode == Log::Mode::DEFERRED)
            stream << logging::keyValue("type", "deferred");
        else if (mode == Log::Mode::IMMEDIATE)
            stream << logging::keyValue("type", "immediate");

        // Remember the index of the message.
        stream << logging::keyValue(
            "index", std::to_string(mThreadData.container->getMessageCount()));
    }
}


void LogTrace::handleLogMessage(
    const Log::Severity severity,
    const Location& location,
    std::string&& messageText)
{
    switch (currentLogMode.getValue())
    {
        case LogTraceMode::Mode::Ignored:
            // Nothing to be done with the message.
            break;

        case LogTraceMode::Mode::Deferred:
            // Add the message for a later display if a LogTrace instance exists. Otherwise fall
            // back to "immediate".
            if (mThreadData.container)
                mThreadData.container->addLogMessage(severity, location, std::move(messageText));
            else
                showLogMessageImmediately(severity, location, messageText);
            break;

        case LogTraceMode::Mode::Immediate:
            showLogMessageImmediately(severity, location, messageText);
            break;
    }
}


void LogTrace::offerImmediateLogMessage(
    Log::Severity severity,
    const Location& location,
    const std::string& message)
{
    if (! isImmediate())
        handleLogMessage(severity, location, std::string(message));
}


void LogTrace::showLogTrace(const Location& location)
{
    // Trigger the display of all deferred trace logs.
    if (mThreadData.container)
        mThreadData.container->doShowLogTrace(location);
}


bool LogTrace::isImmediate()
{
    return currentLogMode.getValue() == LogTraceMode::Mode::Immediate || ! mThreadData.container;
}


LogTrace::FunctionGuard::FunctionGuard(
    const Location& location,
    std::string&& functionName,
    const Log::Severity severity)
  : mFunctionName(std::move(functionName)),
    mSeverity(severity)
{
    TLOGV(mSeverity) << "entering '" << mFunctionName << "()'" << location;
}


LogTrace::FunctionGuard::~FunctionGuard()
{
    if (std::uncaught_exceptions() == 0)
    {
        // Write an exit message when there is no exception.
        TLOGV(mSeverity) << "exiting '" << mFunctionName << "()'";
    }
    else
    {
        // Report the exception and print out all collected trace logs.
        TLOG(ERROR) << "detected an exception while exiting '" << mFunctionName << "()'";
    }
}


LogTrace::Container::Container()
  : mMessages(),
    mMaxMessageCount(getMaxMessageCount()),
    mCutOffMessageCount(0)
{
}


void LogTrace::Container::addLogMessage(
    Log::Severity severity,
    const Location& location,
    std::string&& message)
{
    // Make room for the new message.
    while (mMessages.size() >= mMaxMessageCount)
    {
        mMessages.erase(mMessages.begin());
        ++mCutOffMessageCount;
    }

    mMessages.emplace_back(severity, location, std::move(message));
}


void LogTrace::Container::doShowLogTrace(const Location& location)
{
    auto [messages, cutOffMessageCount] = detachMessages();
    Log::Severity introductionSeverity = determineLogTraceIntroductionSeverity(messages);

    if (! messages.empty())
    {
        Log log(introductionSeverity, location, Log::Mode::STRING);
        log.stream() << "trace log of " << messages.size() << " recorded message"
                     << (messages.size() == 1 ? "" : "s") << getCutOffString(cutOffMessageCount);
        const auto severity = Log::getGoogleLogSeverity(introductionSeverity);
        google::LogMessage(location.file(), location.line(), severity).stream()
            << log.getImplementation().getLogLine();
    }

    size_t index = 1; // Use a 1-based index to allow messages like "1/n" to "n/n".
    const auto countString = std::to_string(messages.size());
    for (auto& message : messages)
    {
        prefixTraceMessage(message.message, index, countString);
        ++index;
        const auto severity = Log::getGoogleLogSeverity(message.severity);
        google::LogMessage(message.location.file(), message.location.line(), severity).stream()
            << message.message;
    }
}


size_t LogTrace::Container::getMessageCount() const
{
    return mMessages.size();
}


LogTrace::Container::Message::Message(
    Log::Severity severity,
    const Location& location,
    std::string&& message)
  : severity(severity),
    location(location),
    message(std::move(message))
{
}


std::pair<std::list<LogTrace::Container::Message>, size_t> LogTrace::Container::detachMessages()
{
    std::list<Message> messages;
    mMessages.swap(messages);
    size_t cutOffMessageCount = mCutOffMessageCount;
    mCutOffMessageCount = 0;
    return std::make_pair(std::move(messages), cutOffMessageCount);
}


Log::Severity LogTrace::Container::determineLogTraceIntroductionSeverity(
    const std::list<Message>& messages)
{
    if (messages.empty())
    {
        // When there is no message then show the introduction at INFO level.
        // The introduction is not skipped, to make it clear that there are no messages
        // and not a problem with logging, flushing etc.
        return Log::Severity::INFO;
    }

    // Determine the maximum severity of all recorded trace messages and use that for the
    // introduction message.
    Log::Severity introductionSeverity = Log::Severity::DEBUG4;
    for (const auto& message : messages)
        if (message.severity < introductionSeverity)
            introductionSeverity = message.severity;
    return introductionSeverity;
}


void LogTrace::showLogMessageImmediately(
    const Log::Severity severity,
    const Location& location,
    const std::string& messageText)
{
    // Immediate display of the message either because it was explicitly configured or
    // because deferred display failed.
    google::LogMessage log(
        location.file(), static_cast<int>(location.line()), Log::getGoogleLogSeverity(severity));
    log.stream() << messageText;
}


thread_local LogTrace::ThreadData LogTrace::mThreadData;

} // namespace epa
