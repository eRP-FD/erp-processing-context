/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_DURATIONCONSUMER_HXX
#define ERP_PROCESSING_CONTEXT_DURATIONCONSUMER_HXX

class DurationConsumer;

#include <chrono>
#include <functional>
#include <optional>
#include <string>


/**
 * Measure how much time passes between construction and destruction.
 * Short lived, there may be multiple measurements per request.
 */
class DurationTimer
{
public:
    using Receiver = std::function<void(
        std::chrono::system_clock::duration duration,
        const std::string& description,
        const std::string& sessionIdentifier)>;

    explicit DurationTimer (
        Receiver& receiver,
        const std::string& description,
        const std::string& sessionIdentifier);
    ~DurationTimer (void);

    void notifyFailure(const std::string& description);

private:
    Receiver& mReceiver;
    std::string mDescription;
    std::string mSessionIdentifier;
    std::optional<std::chrono::system_clock::time_point> mStart;
};


/**
 * An abstraction of X-Request-Id for classes and methods which are relatively far away (in the sense of the call tree)
 * from a triggering request processing. The purpose of this class is to allow an association of a currently processed
 * request and its X-Request-Id with a utility function many stack frames down that, for example, retrieves an OCSP response.
 */
class DurationConsumer
{
public:
    static DurationConsumer& getCurrent (void);


    void initialize (
        const std::string& sessionIdentifier,
        DurationTimer::Receiver&& receiver);
    void reset (void);
    bool isInitialized (void) const;
    std::optional<std::string> getSessionIdentifier (void) const;

    /**
     * Return a timer that measures how long it takes until its destructor is called.
     */
    DurationTimer getTimer (const std::string& description);
    // variant for a timer with a custom receiver/log level
    DurationTimer getTimer (const std::string& description, DurationTimer::Receiver& receiver) const;

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
    DurationConsumerGuard (
        const std::string& sessionIdentifier,
        DurationTimer::Receiver&& receiver);
    ~DurationConsumerGuard (void);
};


#endif
