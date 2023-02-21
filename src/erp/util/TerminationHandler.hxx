/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_TERMINATIONHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_TERMINATIONHANDLER_HXX

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

template <typename>
class PeriodicTimer;

namespace boost::asio
{
class io_context;
}


/**
 * The purpose of the termination handler is to call registered callbacks right before the application exits
 * so that they can release resources.
 * This releasing of resources may require some attention in the future to better handle cases where the application
 * is terminated due to a "serious" signal. In that case some or many system calls must not be called and other restrictions
 * apply as well. At the moment this class is a "best effort". Try to shut down as cleanly as possible while being
 * aware, that that may fail.
 *
 * The TerminationHandler doubles as condition to wait on before existing the application. Typical use:
 *
 *     int main (...)
 *     {
 *         ...initialize application... during which
 *            TerminationHandler::instance().registerCallback( for some resource) for all resources that require clean shut down
 *
 *          // after main loop terminated:
 *          TerminationHandler::instance().notifyTerminationCallbacks(signalHandler.mSignal != SIGTERM);
 *     }
 *
 */
class TerminationHandler
{
public:
    static inline TerminationHandler& instance (void) {return *rawInstance();}
    virtual ~TerminationHandler();

    enum class State
    {
        Running,
        Terminating,
        Terminated
    };

    std::optional<size_t> registerCallback (std::function<void(bool hasError)>&& callback);

    /**
     * Notify that the termination of the application has been requested. This will
     * 1. put it into state==Terminating
     * 2. call all registered callbacks
     * 3. put it into state==Terminated
     *
     * The name of this method could be improved. It is usually to trigger the application exist but relies on
     * the registered callbacks to each do their part of shutting down some resources. It does not, and nor should its
     * callbacks, call exit() or abort(), etc.
     */
    void notifyTerminationCallbacks(bool hasError);

    /// @brief trigger a process termination from anywhere
    virtual void terminate();

    virtual void gracefulShutdown(boost::asio::io_context& ioContext, int delaySeconds);

    State getState (void) const;

    /**
     * Return whether `notifyTerminationCallbacks(bool hasError)` has been called with `hasError==true`.
     */
    bool hasError (void) const;

    bool isShuttingDown() const;

protected:
    /**
     * The constructor is protected so that tests can instantiate it in temporary variables.
     */
    TerminationHandler (void);

private:
    mutable std::mutex mMutex;
    std::vector<std::function<void(bool)>> mCallbacks;
    std::atomic<State> mState;
    std::atomic_bool mHasError;
    class ShutdownDelayTimerHandler;
    using ShutdownDelayTimer= PeriodicTimer<ShutdownDelayTimerHandler>;
    std::unique_ptr<ShutdownDelayTimer> mShutdownDelayTimer;
    class CountDownTimerHandler;
    using CountDownTimer = PeriodicTimer<CountDownTimerHandler>;
    std::unique_ptr<CountDownTimer> mCountDownTimer;

    static std::unique_ptr<TerminationHandler>& rawInstance (void);

    State setState (State newState);
    void callTerminationCallbacks (void);

    /**
     * Tests present an atypical use case for the TerminationHandler:
     * - it is a singleton by nature so that a signal handler can reach and trigger it
     * - its life time extends that of most other objects because it controls the life time of long running objects
     *   like HttpsServer and ApplicationHealthAndRegistrationUpdater
     * - it is supposed to be destroyed when the application is exited.
     *
     * Especially the last item conflicts with the use in tests as each test represents essentially a very simplified run
     * of the application. At whose end the TerminationHandler is *not* destroyed. As a result, we need some
     * special handling for tests in a form that does not compromise its functionality in a production scenario.
     */
    friend class MockTerminationHandler;
    static void setRawInstance (std::unique_ptr<TerminationHandler>&& newInstance);
};


#endif
