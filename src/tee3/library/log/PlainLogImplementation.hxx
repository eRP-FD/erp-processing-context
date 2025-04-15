/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_LOG_PLAINLOGIMPLEMENTATION_HXX
#define EPA_LIBRARY_LOG_PLAINLOGIMPLEMENTATION_HXX

#include "library/log/LogContext.hxx"
#include "library/log/Logging.hxx"

#include <chrono>

namespace epa
{

/**
 * Produce a plain text log message.
 * All data that is appended to `stream()` makes up the message part of the log message.
 * For plain text messages there is little else, save `severity` and `location`, which are
 * handled by the underlying GLog framework. For the derived JsonLogImplementation this distinction
 * is more important.
 */
class PlainLogImplementation : public Log::Implementation, public Log::LogStream::UserData
{
public:
    using ClockType = std::chrono::high_resolution_clock;

    PlainLogImplementation(Log::Severity severity, const Location& location, Log::Mode mode);

    /**
     * Return the stream object which collects the message text.
     */
    Log::LogStream& stream() override;

    /**
     * Key/value pairs are ignored for plain text messages.
     */
    void addKeyValue(const Log::KeyValue& value) override;

    /**
     * JSON serializers are ignored for plain text messages.
     */
    void addJsonSerializer(const Log::JsonSerializer&) override;

    void setLocation(const Location& location) override;

    /**
     * Assemble the log entry and use GLog to output it to console and/or log file.
     */
    void outputLog(bool allowTraceForImmediate) override;

    /**
     * Return the message text for the specified context.
     */
    std::string getMessage(LogContext logContext) const override;

    /**
     * Return the complete log line as it would be handed over to GLog.
     */
    std::string getLogLine() const override;

    /**
     * Returns `false`.
     */
    bool isJson() const override;

    void setFlag(Log::Flag flag) override;

protected:
    const Log::Severity mSeverity;
    const Log::Mode mMode;
    const ClockType::time_point mTime;

    /**
     * Return the string that will be passed on to GLog.
     */
    virtual std::string assembleLogLine() const;

    const Location& getLocation() const;

private:
    Location mLocation;
    Log::LogStream mMessage;
    bool mIsMessageCreated;
};

} // namespace epa

#endif
