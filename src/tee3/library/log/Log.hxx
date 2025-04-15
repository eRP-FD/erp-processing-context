/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_LOG_LOG_HXX
#define EPA_LIBRARY_LOG_LOG_HXX

#include "library/log/EnvironmentValue.hxx"
#include "library/log/LogContext.hxx"
#include "library/wrappers/GLog.hxx"

// We have to define this macro before including fwd.h so that we don't receive errors when
// including the main header.
#ifndef RAPIDJSON_HAS_STDSTRING
#define RAPIDJSON_HAS_STDSTRING 1
#endif
#include <rapidjson/fwd.h>


// Include ClassifiedText.hxx as convenience so that files that use logging only need to
// include Log.hxx.
#include "library/log/ClassifiedText.hxx"

#include <glog/raw_logging.h>
#include <memory>
#include <sstream>
#include <variant>

namespace epa
{

class Location;

/**
 * Each `Log` instance handles a single log message. The format can be either plain text or JSON.
 *
 * Instances are usually created with a macro like `LOG(ERROR)` which injects the current location.
 * Message text is collected with the stream returned by `stream()`. For JSON log messages the message
 * will be placed under the `message` key.
 *
 * Custom key/value pairs can be provided with the `logging::keyValue(key, value)` stream manipulator.
 * Note that they are ignored for plain text messages.
 */
class Log
{
    /**
     * Initialized from environment variable CONFIG_LOG_FORMAT, this variable is `true` when
     * the value is "JSON" (case insensitive) and is `false` for any other value.
     */
public:
    static EnvironmentValue<bool> logFormatIsJson;

    /**
     * Initialized from environment variable LOG_LEVEL, this variable contains the smallest
     * log level that is printed. See the description of `getLogLevel()` below, for a list of values.
     */
    static EnvironmentValue<int> logLevel;

    /**
     * This enum joins the concepts of GLOG's severity (INFO, WARNING, ERROR) and
     * loglevel (used by VLOG). GLOG's reasoning seems to be that VLOG(#) maps to LOG(INFO)
     * if # is lower than or equal to the GLOG_v threshold. So it might be better to think
     * of V# as INFO#. However, the way VLOG is removed from release builds makes V# more like
     * DEBUG#.
     * Therefore, VLOG(#) is mapped to LOG(DEBUG#). Both are removed from non-release
     * builds while LOG(INFO#) is no longer removed from non-release builds.
     */
    enum class Severity // NOLINT(cert-int09-c,readability-enum-initial-value)
    {
        FATAL,
        ERROR,
        WARNING,
        INFO, // synonym for INFO0
        INFO0 = INFO,
        INFO1,
        INFO2,
        INFO3,
        INFO4,
        DEBUG0,
        DEBUG1,
        DEBUG2,
        DEBUG3,
        DEBUG4
    };


    enum class Mode
    {
        /// Print each message when the destructor is called.
        IMMEDIATE,
        /// Collect messages. Print them only when there is an error.
        DEFERRED,
        /// Do not print or collect the message. Only return it via the getString() method.
        STRING
    };


    /**
     * The `KeyValue` class and the static `logging::keyValue()` function are used to
     * provide custom key/value pairs via the stream operator to the currently active
     * `Implementation` object.
     * Used like
     *     LOG(INFO) << "message" << logging::keyValue("key", "value");
     */
    struct KeyValue
    {
        std::string key;
        std::variant<std::string, int64_t, uint64_t, double> value;
    };
    /**
     * The `JsonSerializer` struct wraps a callback that will be called when a `Log` object is being
     * destroyed. The callback will be called with a rapidjson Writer.
     * This allows more complex JSON values than key/value pairs to be written without having to
     * create an intermediate JSON dom.
     */
    struct JsonSerializer
    {
        // We have to explicitly supply all template parameters so that we can get away with
        // rapidjson/fwd.h and don't have to include the RapidJSON source in all files which use
        // logging or assertions.
        using RapidJsonWriter = rapidjson::Writer<
            rapidjson::StringBuffer,
            rapidjson::UTF8<char>,
            rapidjson::UTF8<char>,
            rapidjson::CrtAllocator,
            0>;
        using CallbackType = std::function<void(RapidJsonWriter&)>;
        CallbackType callback;
    };
    // NOLINTNEXTLINE(cppcoreguidelines-virtual-class-destructor): cf. LogStream::UserData
    struct IKeyValueHandler
    {
        virtual void addKeyValue(const KeyValue& keyValue) = 0;
        virtual void addJsonSerializer(const JsonSerializer& serializer) = 0;
    };


    /**
     * This simple class uses polymorphism to provide user data to whoever
     * has access to the `std::ostream` object of the `<<` stream operator. This allows
     * stream manipulators like `logging::keyValue()` or classified text to communicate with
     * e.g. a JsonLogImplementation object.
     * The implementation can generate two different texts (Log- and Response-Text) on demand
     * if classified text is provided. The specific of classified text is that it might have
     * different replacements for log and response.
     */
    class LogStream : public std::stringstream
    {
    public:
        // We have to mark UserData as polymorphic but can not declare the destructor to avoid
        // problems with declaring some `noexcept(false)` and some `noexcept`.
        struct UserData // NOLINT(cppcoreguidelines-virtual-class-destructor)
        {
            virtual void notUsed()
            {
            }
        };

        explicit LogStream(UserData& userData);

        UserData& getUserData();
        void setUserData(UserData& newUserData);
        static UserData* getUserData(std::ostream& os);

        void addClassifiedText(const ClassifiedText& classifiedText);

        /**
         * Returns the log text. The method is supported only for consistency.
         */
        virtual std::string str() const;

        /**
         * Returns a message where ClassifiedText objects have been replaced
         * for use in log messages.
         */
        std::string getLogText() const;

        /**
         * Returns a message where ClassifiedText objects have been replaced
         * for use in response texts.
         */
        std::string getResponseText() const;

    private:
        UserData* mUserData;

        /**
         * In case of classified text the whole text is put to the following variable with the
         * context LogContext::Log. It is still possible that the stream has following normal
         * log text.
         */
        std::string mLogText;

        /**
         * In case of classified text the whole text is put to the following variable with the
         * context LogContext::Response. It is still possible that the stream has following normal
         * log text.
         */
        std::string mResponseText;
    };

    struct ILocationConsumer
    {
        virtual ~ILocationConsumer() = default;
        virtual void setLocation(const Location& location) = 0;
    };

    /**
     * Behavior of the Log class can be influenced with these flags.
     */
    enum class Flag
    {
        ShowSessionDescription
    };
    struct IFlagConsumer
    {
        virtual ~IFlagConsumer() = default;
        virtual void setFlag(Flag flag) = 0;
    };

    /**
     * The implementation differs for plain text and JSON messages.
     */
    class Implementation : public IKeyValueHandler, public ILocationConsumer, IFlagConsumer
    {
    public:
        ~Implementation() override = default;
        virtual Log::LogStream& stream() = 0;
        virtual void outputLog(bool allowTraceForImmediate) = 0;
        virtual std::string getMessage(LogContext) const = 0;
        virtual std::string getLogLine() const = 0;
        virtual bool isJson() const = 0;
        void setFlag(Flag flag) override = 0;
    };


    Log(const Severity severity, const Location& location, Mode mode = Mode::IMMEDIATE);
    ~Log();

    /**
     * The content of this stream will become the value of the JSON "message" field.
     */
    LogStream& stream();

    /**
     * Return the message related to specified log context.
     */
    std::string getMessage(LogContext logContext) const;

    /**
     * Output the log entry to the underlying logger (GLog).
     * By default, `allowTraceForImmediate` allows messages with `mode==Immediate` to be
     * added to the log trace as well. When `false` then no trace log is created for it.
     */
    void outputLog(bool allowTraceForImmediate = true);

    Implementation& getImplementation();

    bool isJson() const;

    void setFlag(const Flag flag);

    /**
      * Map the Severity enum into one of google::INFO, google::WARNING, google::ERROR or google::FATAL.
      */
    static constexpr google::LogSeverity getGoogleLogSeverity(const Log::Severity severity);

    /**
     * Return a numerical log level value for the given `severity` value.
     *
     * The levels are:
     *  3 - FATAL
     *  2 - ERROR
     *  1 - WARNING
     *  0 - INFO,INFO0, DEBUG,DEBUG0, V0
     * -1 - INFO1, DEBUG1, V1
     * -2 - INFO2, DEBUG2, V2
     * -3 - INFO3, DEBUG3, V3
     * -4 - INFO4, DEBUG4, V4
     */
    static constexpr int getLogLevel(Log::Severity severity);

    /**
     * Allow numerical values (positive and negative) as well as symbolic names.
    *  The given `value` is trimmed. Text values are parsed case insensitive.
     * See `getLogLevel` for the list of recognized names.
     * When the given `value` can not be parsed, a warning is logged and DEFAULT_LOG_LEVEL_ENV is
     * used.
     */
    static int parseLogLevel(std::string_view value);

    /**
     * Return a display string for the `Severity` enum. It is used in JSON log messages for the
     * `severity` key.
     */
    static constexpr std::string_view getSeverityString(const Log::Severity severity);

    /**
     * Apply `level` to `severity`. This only works for `DEBUG` and `INFO`.
     * E.g severity=DEBUG and level=2 is mapped to DEBUG2.
     */
    static constexpr Severity getLeveledSeverity(Log::Severity severity, int level);

private:
    std::unique_ptr<Implementation> mImplementation;
};

std::ostream& operator<<(std::ostream& os, const Log::KeyValue& keyValue);
std::ostream& operator<<(std::ostream& os, const Log::JsonSerializer& jsonSerializer);
std::ostream& operator<<(std::ostream& os, const Location& location);
std::ostream& operator<<(std::ostream& os, const Log::Flag flag);

namespace logging
{
    /**
     * Create a stream manipulator which will add the given `key` and `value` to
     * the current JSON log message. For plain text messages, `key` and `value` are discarded.
     * Add support for additional value types as needed.
     */
    Log::KeyValue keyValue(const std::string& key, const std::string& value);
    Log::KeyValue keyValue(const std::string& key, int64_t value);
    Log::KeyValue keyValue(const std::string& key, uint64_t value);
    Log::KeyValue keyValue(const std::string& key, double value);

    Log::Flag withFlag(Log::Flag flag);

    Log::JsonSerializer jsonWriter(Log::JsonSerializer::CallbackType&& callback);


    //NOLINTNEXTLINE(clang-diagnostic-unneeded-internal-declaration)
    static inline const std::string defaultReplacement = "****";

    /**
     * Mark `value` as (contributing to) identifying a person.
     * Examples:
     *    KVNR
     *    Name
     */
    template<typename T>
    ClassifiedText personal(T&& value, std::string replacement = defaultReplacement);

    /**
     * Mark `value` as sensitive information about a person.
     * Examples:
     *     Name of illness
     *
     * If in doubt whether information is personal or sensitive, use sensitive.
     */
    template<typename T>
    ClassifiedText sensitive(T&& value, std::string replacement = defaultReplacement);

    /**
     * Mark `value` as confidential information of the provider or the platform.
     * Examples:
     *     Certificate
     *
     * Passwords are not just confidential and should never be displayed.
     */
    template<typename T>
    ClassifiedText confidential(T&& value, std::string replacement = defaultReplacement);

    /**
     * The given `value` as always shown without modification.
     * Use this text classification in cases where the type `ClassifiedText` is required but the
     * `value` can be shown in any environment. This is the case in ternary expressions where
     * in one branch a classified text is used but the other contains only plain text. E.g:
     * ```
     * LOG(INFO) << (condition ? personal("personal information") : plain("not personal"));
     * ```
     */
    template<typename T>
    ClassifiedText plain(T&& value);

    /**
     * Mark `value` as used only for development.
     * Never mark information about a real person or a production environment as `development`.
     * This mark should be used for synthetic test data. Use in production builds will lead to
     * warnings.
     */
    template<typename T>
    ClassifiedText development(T&& value, std::string replacement = defaultReplacement);
}


namespace logging
{
    template<typename T>
    ClassifiedText personal(T&& value, std::string replacement)
    {
        return ClassifiedText::create(
            std::forward<T>(value),
            std::move(replacement),
            ClassifiedText::Visibility::ShowInResponse);
    }

    template<typename T>
    ClassifiedText sensitive(T&& value, std::string replacement)
    {
        return ClassifiedText::create(
            std::forward<T>(value),
            std::move(replacement),
            ClassifiedText::Visibility::ShowInResponse);
    }

    template<typename T>
    ClassifiedText confidential(T&& value, std::string replacement)
    {
        return ClassifiedText::create(
            std::forward<T>(value), std::move(replacement), ClassifiedText::Visibility::ShowInLog);
    }

    template<typename T>
    ClassifiedText plain(T&& value)
    {
        return ClassifiedText::create(
            std::forward<T>(value), "", ClassifiedText::Visibility::ShowAlways);
    }

    template<typename T>
    ClassifiedText development(T&& value, std::string replacement)
    {
        return ClassifiedText::create(
            std::forward<T>(value),
            std::move(replacement),
            ClassifiedText::Visibility::ShowInDevelopmentBuild);
    }
}


constexpr google::LogSeverity Log::getGoogleLogSeverity(const Log::Severity severity)
{
    switch (severity)
    {
        case Log::Severity::FATAL:
            return google::GLOG_FATAL;
        case Log::Severity::ERROR:
            return google::GLOG_ERROR;
        case Log::Severity::WARNING:
            return google::GLOG_WARNING;

        case Log::Severity::INFO0:
        case Log::Severity::INFO1:
        case Log::Severity::INFO2:
        case Log::Severity::INFO3:
        case Log::Severity::INFO4:
        case Log::Severity::DEBUG0:
        case Log::Severity::DEBUG1:
        case Log::Severity::DEBUG2:
        case Log::Severity::DEBUG3:
        case Log::Severity::DEBUG4:
            return google::GLOG_INFO;

        default:
            return google::GLOG_WARNING;
    }
}


constexpr int Log::getLogLevel(const Log::Severity severity)
{
    switch (severity)
    {
        case Log::Severity::FATAL:
            return google::GLOG_FATAL;
        case Log::Severity::ERROR:
            return google::GLOG_ERROR;
        case Log::Severity::WARNING:
            return google::GLOG_WARNING;

        case Log::Severity::INFO0:
        case Log::Severity::DEBUG0:
            return 0;

        case Log::Severity::INFO1:
        case Log::Severity::DEBUG1:
            return -1;

        case Log::Severity::INFO2:
        case Log::Severity::DEBUG2:
            return -2;

        case Log::Severity::INFO3:
        case Log::Severity::DEBUG3:
            return -3;

        case Log::Severity::INFO4:
        case Log::Severity::DEBUG4:
            return -4;

        default:
            return google::GLOG_WARNING;
    }
}


constexpr std::string_view Log::getSeverityString(const Log::Severity severity)
{
    switch (severity)
    {
        case Log::Severity::FATAL:
            return "FATAL";
        case Log::Severity::ERROR:
            return "ERROR";
        case Log::Severity::WARNING:
            return "WARNING";

        case Log::Severity::INFO0:
            return "INFO0";
        case Log::Severity::INFO1:
            return "INFO1";
        case Log::Severity::INFO2:
            return "INFO2";
        case Log::Severity::INFO3:
            return "INFO3";
        case Log::Severity::INFO4:
            return "INFO4";

        case Log::Severity::DEBUG0:
            return "DEBUG0";
        case Log::Severity::DEBUG1:
            return "DEBUG1";
        case Log::Severity::DEBUG2:
            return "DEBUG2";
        case Log::Severity::DEBUG3:
            return "DEBUG3";
        case Log::Severity::DEBUG4:
            return "DEBUG4";

        default:
            return "[unknown] ";
    }
}


constexpr Log::Severity Log::getLeveledSeverity(const Log::Severity severity, const int level)
{
    if (severity == Log::Severity::DEBUG0)
    {
        switch (level)
        {
            case 0:
                return Log::Severity::DEBUG0;
            case 1:
                return Log::Severity::DEBUG1;
            case 2:
                return Log::Severity::DEBUG2;
            case 3:
                return Log::Severity::DEBUG3;
            case 4:
                return Log::Severity::DEBUG4;
            default:; // fall through to the warning at the end of this method.
        }
    }
    else if (severity == Log::Severity::INFO0)
    {
        switch (level)
        {
            case 0:
                return Log::Severity::INFO0;
            case 1:
                return Log::Severity::INFO1;
            case 2:
                return Log::Severity::INFO2;
            case 3:
                return Log::Severity::INFO3;
            case 4:
                return Log::Severity::INFO4;
            default:; // fall through to the warning at the end of this method.
        }
    }

    RAW_LOG(
        WARNING,
        "WARNING: can only level severities INFO and DEBUG, not %d",
        static_cast<int>(severity));

    return severity;
}

} // namespace epa

#endif
