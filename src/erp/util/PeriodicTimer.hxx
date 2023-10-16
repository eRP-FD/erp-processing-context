/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_PERIODICTIMER_HXX
#define ERP_PROCESSING_CONTEXT_PERIODICTIMER_HXX

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>


class PeriodicTimerBase;

class TimerHandler
{
public:
    virtual void timerHandler() = 0;
    virtual std::optional<std::chrono::steady_clock::duration> nextInterval() const = 0;
    virtual ~TimerHandler() noexcept = default;

private:
    std::mutex mMutex;
    bool canceled = false;
    friend class PeriodicTimerBase;
};

class OneShotHandler : public TimerHandler
{
public:
    std::optional<std::chrono::steady_clock::duration> nextInterval() const final;
};

class FixedIntervalHandler : public TimerHandler
{
public:
    FixedIntervalHandler(std::chrono::steady_clock::duration interval);
    std::optional<std::chrono::steady_clock::duration> nextInterval() const final;

private:
    const std::chrono::steady_clock::duration mInterval;
};


class PeriodicTimerBase
{
public:
    using HandlerSharedPtr = std::shared_ptr<TimerHandler>;
    PeriodicTimerBase(HandlerSharedPtr timerHandler);
    virtual ~PeriodicTimerBase();

    void start(boost::asio::io_context& context, std::chrono::steady_clock::duration initialInterval);
    void cancel();

    PeriodicTimerBase(const PeriodicTimerBase&) = delete;
    PeriodicTimerBase(PeriodicTimerBase&&) = delete;
    PeriodicTimerBase& operator=(const PeriodicTimerBase&) = delete;
    PeriodicTimerBase& operator=(PeriodicTimerBase&&) = delete;

private:
    using TimerSharedPtr = std::unique_ptr<boost::asio::steady_timer>;
    void timerHandlerInternal(const HandlerSharedPtr& handler,
                                     const boost::system::error_code& errorCode);
    auto timerCallback(const HandlerSharedPtr& handler);
    TimerSharedPtr mTimer;

protected:
    std::shared_ptr<TimerHandler> mTimerHandler;
};

template<typename TimerHandlerT>
class PeriodicTimer : public PeriodicTimerBase
{
    static_assert(std::is_base_of_v<TimerHandler, TimerHandlerT>, "TimerHandlerT must be derived from TimerHandler");

public:
    // constructor that constructs the Handler by forwarding all arguments to the constructor of TimerHandlerT.
    template<typename... ArgTs>
    explicit PeriodicTimer(ArgTs&&... args)
        requires requires { TimerHandlerT{std::forward<ArgTs>(std::declval<ArgTs>())...}; };

    template<typename DerivedHandlerT, typename... ArgTs>
    explicit PeriodicTimer(std::shared_ptr<DerivedHandlerT>)
        requires std::is_base_of_v<TimerHandlerT, DerivedHandlerT>;

    using PeriodicTimerBase::start;

    std::shared_ptr<TimerHandlerT> handler();
    std::shared_ptr<const TimerHandlerT> handler() const;
};

template<typename TimerHandlerT>
template<typename... ArgTs>
PeriodicTimer<TimerHandlerT>::PeriodicTimer(ArgTs&&... args)
    requires requires { TimerHandlerT{std::forward<ArgTs>(std::declval<ArgTs>())...}; }
    : PeriodicTimerBase{std::make_shared<TimerHandlerT>(std::forward<ArgTs>(args)...)}
{
}
template<typename TimerHandlerT>
template<typename DerivedHandlerT, typename... ArgTs>
PeriodicTimer<TimerHandlerT>::PeriodicTimer(std::shared_ptr<DerivedHandlerT> derivedHandler)
    requires std::is_base_of_v<TimerHandlerT, DerivedHandlerT>
    : PeriodicTimerBase{std::move(derivedHandler)}
{}

template<typename TimerHandlerT>
std::shared_ptr<TimerHandlerT> PeriodicTimer<TimerHandlerT>::handler()
{
    return static_pointer_cast<TimerHandlerT>(mTimerHandler);
}

template<typename TimerHandlerT>
std::shared_ptr<const TimerHandlerT> PeriodicTimer<TimerHandlerT>::handler() const
{
    return static_pointer_cast<const TimerHandlerT>(mTimerHandler);
}


#endif// ERP_PROCESSING_CONTEXT_PERIODICTIMER_HXX
