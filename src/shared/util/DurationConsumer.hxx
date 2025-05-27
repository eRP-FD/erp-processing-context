/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_DURATIONCONSUMER_HXX
#define ERP_PROCESSING_CONTEXT_DURATIONCONSUMER_HXX

class DurationConsumer;

#include "JsonLog.hxx"

#include <chrono>
#include <functional>
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
        const std::string& category,
        const std::string& metric,
        const std::string& sessionIdentifier,
        const std::unordered_map<std::string, std::string>& keyValueMap,
        const std::optional<JsonLog::LogReceiver>& logReceiverOverride)>;

    explicit DurationTimer (
        Receiver& receiver,
        std::string  category,
        std::string  metric,
        std::string  sessionIdentifier,
        const std::unordered_map<std::string, std::string>& keyValueMap);
    ~DurationTimer();

    void notifyFailure(const std::string& description);
    void keyValue(const std::string& key, const std::string& value);

private:
    Receiver mReceiver;
    std::string mCategory;
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
    // Keep these names in sync with the configuration:
    static constexpr auto categoryRedis = "redis";
    static constexpr auto categoryPostgres = "postgres";
    static constexpr auto categoryUrlRequestSender = "httpclient";
    static constexpr auto categoryFhirValidation = "fhirvalidation";
    static constexpr auto categoryOcspRequest = "ocsprequest";
    static constexpr auto categoryHsm = "hsm";
    static constexpr auto categoryEnrolmentHelper = "enrolment";

    static DurationConsumer& getCurrent();


    void initialize(const std::string& sessionIdentifier, DurationTimer::Receiver&& receiver);
    void reset() noexcept;
    bool isInitialized() const;
    std::optional<std::string> getSessionIdentifier() const;

    /**
     * @brief
     * Default implementation of DurationTimer::Receiver
     */
    static void defaultReceiver(
        std::chrono::steady_clock::duration duration,
        const std::string& category,
        const std::string& metric,
        const std::string& sessionIdentifier,
        const std::unordered_map<std::string, std::string>& keyValueMap,
        const std::optional<JsonLog::LogReceiver>& logReceiverOverride);

    /**
     * Return a timer that measures how long it takes until its destructor is called.
     */
    DurationTimer getTimer(const std::string& category, const std::string& metric,
                           const std::unordered_map<std::string, std::string>& keyValueMap = {});

private:
    bool mIsInitialized = false;
    std::optional<std::string> mSessionIdentifier;
    DurationTimer::Receiver mReceiver;
};


/**
 * Life time control of active DurationConsumer objects. Each one is defined per-thread but its life time must be
 * limited to that of the current (per thread) request being processed.
 */
class DurationConsumerGuard
{
public:
    explicit DurationConsumerGuard (
        const std::string& sessionIdentifier,
        DurationTimer::Receiver&& receiver = nullptr);

    ~DurationConsumerGuard (void);
};


#endif
