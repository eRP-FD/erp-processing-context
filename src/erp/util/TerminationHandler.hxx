#ifndef ERP_PROCESSING_CONTEXT_TERMINATIONHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_TERMINATIONHANDLER_HXX

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>


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
 *         TerminationHandler::instance().waitForTerminated();
 *         return TerminationHandler::instance().hasError() ? EXIT_FAILURE : EXIT_SUCCESS;
 *     }
 *
 */
class TerminationHandler
{
public:
    static inline TerminationHandler& instance (void) {return *rawInstance();}

    enum class State
    {
        Running,
        Terminating,
        Terminated
    };

    /**
     * The destructor will trigger termination, if that has not yet happened, and then wait for the termination
     * thread to finish calling all its callbacks.
     */
    virtual ~TerminationHandler (void);

    virtual std::optional<size_t> registerCallback (std::function<void(bool hasError)>&& callback);

    /**
     * Notify that the termination of the application has been requested. This will
     * 1. put it into state==Terminating
     * 2. call all registered callbacks
     * 3. put it into state==Terminated
     * 4. notify all threads that wait in waitForTerminated
     *
     * The name of this method could be improved. It is usually to trigger the application exist but relies on
     * the registered callbacks to each do their part of shutting down some resources. It does not, and nor should its
     * callbacks, call exit() or abort(), etc.
     */
    void notifyTermination (bool hasError);

    /**
     * Block the calling thread until the application is terminated via a call to `notifyTermination`, either because
     * of an error or because of an explicit shutdown. I.e. until State == Terminated
     *
     */
    virtual void waitForTerminated (void);

    virtual State getState (void) const;

    /**
     * Return whether `notifyTermination(bool hasError)` has been called with `hasError==true`.
     */
    virtual bool hasError (void) const;

protected:
    /**
     * The constructor is protected so that tests can instantiate it in temporary variables.
     */
    TerminationHandler (void);

private:
    mutable std::mutex mMutex;
    std::condition_variable mTerminationCondition;
    std::vector<std::function<void(bool)>> mCallbacks;
    std::unique_ptr<std::thread> mTerminationThread;
    State mState;
    bool mHasError;

    static std::unique_ptr<TerminationHandler>& rawInstance (void);

    State setState (State newState);
    void callTerminationCallbacks (void);

    /**
     * Tests present an atypical use case for the TerminationHandler:
     * - it is a singleton by nature so that a signal handler can reach and trigger it
     * - its life time extends that of most other objects because it controls the life time of long running objects
     *   like HttpsServer and HeartbeatSender
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
