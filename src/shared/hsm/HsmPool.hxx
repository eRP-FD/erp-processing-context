/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_HSMPOOL_HXX
#define ERP_PROCESSING_CONTEXT_HSMPOOL_HXX

#include "shared/crypto/RandomSource.hxx"
#include "shared/hsm/HsmPoolSession.hxx"
#include "shared/hsm/HsmFactory.hxx"
#include "shared/hsm/TeeTokenUpdater.hxx"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <list>


/**
 * Opening and closing connections to the HSM server is expensive. Therefore we maintain a pool of HsmSession objects.
 *
 * Acquire an HsmPoolSession object with the `acquire()` method. An explicit call to `release()` is not necessary. Just destroy the
 * session object.
 * HsmPoolSession is a thin wrapper around a nested HsmSession object that will do the cleanup on destruction. The nested HsmSession
 * object is not destroyed but returned into the pool.
 *
 *
 * When during the processing of a request the first call to the HSM is made, an instance is retrieved from the pool.
 * If there is no available instance then a new one is created. This means that a maximum number of
 * number-of-request-processing-threads + 1 instances could be active at the same time. The '+ 1' for the
 * reseeding of the openssl PRNGs, the rest for the request processor threads.
 */
class HsmPool
{
public:
    explicit HsmPool (
        std::unique_ptr<HsmFactory>&& hsmFactory,
        const TeeTokenUpdater::TeeTokenUpdaterFactory& teeTokenUpdaterFactory,
        std::shared_ptr<Timer> timerManager);

    ~HsmPool();

    /**
     * This method should be called once when the application terminates to release any inactive sessions.
     * Sessions that are still active at this point will be released when they are released with the `release()`
     * method.
     *
     * Noexcept because this method can be called from the TerminationHandler as a result of a signal.
     * An exception would be bad in this circumstance.
     *
     * Note that this method will try to release inactive sessions but not the active ones.
     */
    void releasePool (void) noexcept;

    /**
     * Return a connected HsmSession object. This comes either from the pool, or, if that is empty, is created.
     *
     * In order to release the HSM session it is enough to just destroy the returned HsmPoolSession object. It will
     * move its HsmSession core back into the pool.
     */
    HsmPoolSession acquire (void);

    /**
     * This is a method that is supposed to be called only from HsmPoolSession.
     * Are we all well behaved or do we need a mechanism that enforces this?
     */
    void release (std::unique_ptr<HsmSession>&& session);

    void setTeeToken (ErpBlob&& teeToken);

    size_t activeSessionCount (void) const;
    size_t inactiveSessionCount (void) const;

    size_t maxUsedSessionCount (void) const;
    void resetMaxUsedSessionCount (void);

    const TeeTokenUpdater& getTokenUpdater() const;
    TeeTokenUpdater& getTokenUpdater();

    bool isKeepAliveJobRunning (void) const;

    HsmFactory& getHsmFactory();

protected: // protected so that we can override some functionality in tests.
    mutable std::mutex mMutex;
    std::list<std::unique_ptr<HsmSession>> mAvailableHsmSessions;
    size_t mActiveSessionCount;
    bool mIsPoolReleased;
    std::condition_variable mWaitForAvailableSessionVariable;
    std::unique_ptr<HsmFactory> mHsmFactory;
    std::shared_ptr<HsmPoolSessionRemover> mSessionRemover;
    ErpBlob mTeeToken;
    std::unique_ptr<TeeTokenUpdater> mTokenUpdater;
    Timer::JobToken mKeepAliveUpdateToken;
    std::chrono::system_clock::duration mHsmIdleTimeout;

private:
    /**
     * The maximum number of concurrently active sessions. When more than this number of sessions are acquired,
     * then calls will wait until an active session is released and becomes available.
     */
    const size_t mMaxSessionCount;
    /**
     * For testing: the maximum number of concurrently used sessions. Can be used to verify that mMaxSessionCount
     * is obeyed.
     */
    size_t mMaxUsedSessionCount;

    std::shared_ptr<Timer> mTimerManager;

    HsmPoolSession activateSession (void);

    /**
     * For all sessions in mAvailableHsmSession run a command against the HSM to prevent the HSM session from timing out.
     *
     * The caller is expected to have `mMutex` locked.
     */
    void keepAvailableHsmSessionsAlive (void);

    /**
     * Hsm sessions that are returned to the set of available sessions are expected to have just been used to make a call
     * to the HSM. In case that turns out not to be true, make a call to the HSM to prevent the returned session
     * from timing out.
     *
     * The caller is expected to be the owner of `session`. But `mMutex` does not need to be locked.
     */
    void keepHsmSessionAlive (HsmSession& session);
};

#endif
