/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/log/PlainLogImplementation.hxx"
#include "library/wrappers/GLog.hxx"

namespace epa
{

PlainLogImplementation::PlainLogImplementation(
    const Log::Severity severity,
    const Location& location,
    const Log::Mode mode)
  : mSeverity(severity),
    mMode(mode),
    mTime(ClockType::now()),
    mLocation(location),
    mMessage(*this),
    mIsMessageCreated(false)
{
}


void PlainLogImplementation::outputLog(const bool allowTraceForImmediate)
{
    if (mIsMessageCreated)
        return;
    else
        mIsMessageCreated = true;

    if (mMode != Log::Mode::STRING)
        LogTrace::prepareMessage(mMessage, mMode);

    std::string logLine = getLogLine();

    if (mMode == Log::Mode::IMMEDIATE)
    {
        google::LogMessage log(
            mLocation.file(),
            static_cast<int>(mLocation.line()),
            Log::getGoogleLogSeverity(mSeverity));
        log.stream() << logLine;

        // Add the immediate log message also to the deferred trace log messages.
        if (allowTraceForImmediate)
            LogTrace::offerImmediateLogMessage(mSeverity, mLocation, logLine);
    }
    else if (mMode == Log::Mode::DEFERRED)
    {
        LogTrace::handleLogMessage(mSeverity, mLocation, std::move(logLine));
    }
    // nothing to be done for Log::Mode::String
}


Log::LogStream& PlainLogImplementation::stream()
{
    return mMessage;
}


void PlainLogImplementation::addKeyValue(const Log::KeyValue&)
{
    // Key/Value pairs are not supported for plain text messages.
}


void PlainLogImplementation::addJsonSerializer(const Log::JsonSerializer&)
{
    // Not supported for plain text messages.
}


void PlainLogImplementation::setLocation(const Location& location)
{
    mLocation = location;
}


std::string PlainLogImplementation::getMessage(LogContext logContext) const
{
    switch (logContext)
    {
        case LogContext::Log:
            return mMessage.getLogText();
        case LogContext::Response:
            return mMessage.getResponseText();
        case LogContext::Unknown:
            return std::string();
    }

    throw std::logic_error("LogContext was extended, but this method was not");
}


std::string PlainLogImplementation::getLogLine() const
{
    return assembleLogLine();
}


bool PlainLogImplementation::isJson() const
{
    return false;
}


void PlainLogImplementation::setFlag(Log::Flag)
{
    // Flags are not supported.
}


std::string PlainLogImplementation::assembleLogLine() const
{
    return getMessage(LogContext::Log);
}


const Location& PlainLogImplementation::getLocation() const
{
    return mLocation;
}

} // namespace epa
