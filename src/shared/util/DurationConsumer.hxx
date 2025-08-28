/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_DURATIONCONSUMER_HXX
#define ERP_PROCESSING_CONTEXT_DURATIONCONSUMER_HXX

class DurationConsumer;

#include "shared/util/DurationCategory.hxx"
#include "shared/util/JsonLog.hxx"

#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>

/**
 * Measure how much time passes between construction and destruction.
 * Short lived, there may be multiple measurements per request.
 */
class DurationTimer
{
public:
    using Receiver = std::function<void(
        std::chrono::steady_clock::duration duration,
        DurationCategory category,
        const std::string& metric,
        const std::string& sessionIdentifier,
        const std::unordered_map<std::string, std::string>& keyValueMap,
        const std::optional<JsonLog::LogReceiver>& logReceiverOverride)>;

    explicit DurationTimer (
        Receiver& receiver,
        DurationCategory category,
        std::string  metric,
        std::string  sessionIdentifier,
        const std::unordered_map<std::string, std::string>& keyValueMap);
    ~DurationTimer();

    void notifyFailure(const std::string& description);
    void keyValue(const std::string& key, const std::string& value);

private:
    Receiver mReceiver;
    DurationCategory mCategory;
    std::string mMetric;
    std::string mSessionIdentifier;
    std::unordered_map<std::string, std::string> mKeyValueMap;
    std::optional<std::chrono::steady_clock::time_point> mStart;
    const int mUncaughtExceptions{std::uncaught_exceptions()};
};


/**
 * An abstraction of X-Request-Id for classes and methods which are relatively far away (in the sense of the call tree)
 * from a triggering request processing. The purpose of this class is to allow an association of a currently processed
 * request and its X-Request-Id with a utility function many stack frames down that, for example, retrieves an OCSP response.
 */
class DurationConsumer
{
public:
    static DurationConsumer& getCurrent();


    void initialize(const std::string& sessionIdentifier, DurationTimer::Receiver&& receiver,
                    std::map<DurationCategory, std::chrono::milliseconds> loggingThresholds);
    void reset() noexcept;
    bool isInitialized() const;
    std::optional<std::string> getSessionIdentifier() const;

    /**
     * @brief
     * Default implementation of DurationTimer::Receiver
     */
    static void defaultReceiver(
        const std::map<DurationCategory, std::chrono::milliseconds>& loggingThresholds,
        std::chrono::steady_clock::duration duration,
        DurationCategory category,
        const std::string& metric,
        const std::string& sessionIdentifier,
        const std::unordered_map<std::string, std::string>& keyValueMap,
        const std::optional<JsonLog::LogReceiver>& logReceiverOverride);

    /**
     * Return a timer that measures how long it takes until its destructor is called.
     */
    DurationTimer getTimer(DurationCategory category, const std::string& metric,
                           const std::unordered_map<std::string, std::string>& keyValueMap = {});

private:
    bool mIsInitialized = false;
    std::optional<std::string> mSessionIdentifier;
    DurationTimer::Receiver mReceiver;
    std::map<DurationCategory, std::chrono::milliseconds> mLoggingThresholds;
};


/**
 * Life time control of active DurationConsumer objects. Each one is defined per-thread but its life time must be
 * limited to that of the current (per thread) request being processed.
 */
class DurationConsumerGuard
{
public:
    explicit DurationConsumerGuard(const std::string& sessionIdentifier,
                                   std::map<DurationCategory, std::chrono::milliseconds>&& loggingThresholds = {},
                                   DurationTimer::Receiver&& receiver = nullptr);

    ~DurationConsumerGuard ();
};


#endif
