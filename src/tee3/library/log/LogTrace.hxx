/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_LOG_LOGTRACE_HXX
#define EPA_LIBRARY_LOG_LOGTRACE_HXX

#include "library/log/Log.hxx"

#include <boost/core/noncopyable.hpp>
#include <list>
#include <mutex>

namespace epa
{

struct LogTraceMode
{
    enum class Mode
    {
        Deferred, // TLOG messages are held back until a call to `showLogTrace()`.
        Ignored,  // TLOG messages are ignored, i.e. never shown.
        Immediate // TLOG messages are immediately shown, i.e. TLOG behaves like LOG.
    };
    static Mode fromString(std::string_view value, std::string_view defaultMode);
    static std::string toString(LogTraceMode::Mode value);
};


/**
 * Virtual container of log messages that are held back until explicitly requested to be printed. If
 * multiple instances of this exist in a thread, only one container is managed internally for that
 * thread. Thus, LogTrace instances can be nested, but it always behaves the same as there were only
 * the outer instance. In addition, the order of destructing nested instances in a thread is not
 * mandatory, thus you can create LogTrace A and then B, and then destruct A before B. Thereby the
 * internal container lives from constructing A until destructing B.
 *
 * The maximum number of pending messages is limited by configuration value
 * CONFIG_TRACE_LOG_MAX_COUNT / CONFIG_JSON_TRACE_LOG_MAX_COUNT / DEFAULT_TRACE_LOG_MAX_COUNT
 * The oldest messages are discarded, when necessary.
 */
class LogTrace : private boost::noncopyable
{
public:
    static EnvironmentValue<LogTraceMode::Mode> currentLogMode;

    LogTrace();
    ~LogTrace();

    /**
     * Prepare a deferred trace message by adding "type":"deferred" and an "index" value to it.
     * This is only done when there is an active LogTrace object.
     */
    static void prepareMessage(Log::LogStream& stream, Log::Mode mode);

    /**
     * Handle a new log message depending on the current `mode` value.
     * When `mode` is `Ignored` then the message is discarded and will never be displayed.
     * When `mode` is `Immediate` or if there is no active LogTrace object, the message is displayed
     * immediately.
     * Otherwise the message is appended to the list of deferred messages and displayed when
     * `showLogTrace()` is called.
     */
    static void handleLogMessage(
        Log::Severity severity,
        const Location& location,
        std::string&& message);

    /**
     * Similar to `handleLogMessage` but adds the offered message only when log tracing is not
     * already in `immediate` mode. This method is intended to be used for `LOG()` messages,
     * not for `TLOG()`. It allows all log messages to be in the trace log in the correct order.
     */
    static void offerImmediateLogMessage(
        Log::Severity severity,
        const Location& location,
        const std::string& message);

    /**
     * Show all pending messages in the order in which they where added.
     * An introductory message is printed first and describes how many messages follow.
     * The given `location` is only used for this introduction. For all other messages, their
     * original locations are printed.
     *
     * When in doubt, use the LOG_TRACE() macro to call this function.
     */
    static void showLogTrace(const Location& location);

    /**
     * There are scenarios in which a trace log message is displayed immediately.
     * Return whether that is currently the case.
     */
    static bool isImmediate();

    /**
     * A simple RAII class that prints one message when a function is entered (or rather when the
     * `FunctionGuard` object is created) and one message when that function is exited. When it is
     * exited as the result of an exception, an error message to that effect is printed instead.
     *
     * Use `TLOG_FUNCTION_GUARD()` to create a instance, it will add both location and function name
     * automatically.
     */
    class FunctionGuard : private boost::noncopyable
    {
    public:
        FunctionGuard(
            const Location& location,
            std::string&& functionName,
            Log::Severity severity = Log::Severity::INFO);
        ~FunctionGuard();

    private:
        const std::string mFunctionName;
        const Log::Severity mSeverity;
    };

private:
    /**
     * Class for collecting and showing the log trace messages of a thread. There is at most one
     * instance for each thread. This class is not thread-safe, but each instance is only accessed
     * by a single thread.
     */
    class Container
    {
    public:
        Container();

        /**
         * Called by TLOG(), this method adds a pending message. The message is only printed when
         * `showLogTrace` is called, typically via `LOG_TRACE()`. The `message` is a preformatted
         * string created by `TLOG()`, whose arguments are evaluated when `TLOG()` is evaluated.
         */
        void addLogMessage(Log::Severity severity, const Location& location, std::string&& message);

        void doShowLogTrace(const Location& location);

        size_t getMessageCount() const;

    private:
        struct Message
        {
            Log::Severity severity;
            Location location;
            std::string message;

            Message(Log::Severity severity, const Location& location, std::string&& message);
        };

        std::pair<std::list<Message>, size_t> detachMessages();

        static Log::Severity determineLogTraceIntroductionSeverity(
            const std::list<Message>& messages);

        std::list<Message> mMessages;
        const size_t mMaxMessageCount;
        size_t mCutOffMessageCount;
    };

    struct ThreadData
    {
        size_t logTraceInstanceCount = 0;
        std::unique_ptr<Container> container;
    };

    static void showLogMessageImmediately(
        const Log::Severity severity,
        const Location& location,
        const std::string& messageText);

    static thread_local ThreadData mThreadData;
};

} // namespace epa

#endif
