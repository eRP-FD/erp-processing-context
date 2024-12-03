/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/hsm/HsmPool.hxx"
#include "shared/deprecated/TerminationHandler.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/TLog.hxx"

#include <iostream>


HsmPool::HsmPool(std::unique_ptr<HsmFactory>&& hsmFactory,
                 const TeeTokenUpdater::TeeTokenUpdaterFactory& teeTokenUpdaterFactory,
                 std::shared_ptr<Timer> timerManager)
    : mMutex()
    , mAvailableHsmSessions()
    , mActiveSessionCount(0)
    , mIsPoolReleased(false)
    , mWaitForAvailableSessionVariable()
    , mHsmFactory(std::move(hsmFactory))
    , mSessionRemover(std::make_shared<HsmPoolSessionRemover>([this](std::unique_ptr<HsmSession>&& session) {
        release(std::move(session));
    }))
    , mTeeToken()
    , mKeepAliveUpdateToken(Timer::NotAJob)
    , mHsmIdleTimeout(
          std::chrono::seconds(Configuration::instance().getIntValue(ConfigurationKey::HSM_IDLE_TIMEOUT_SECONDS)))
    , mMaxSessionCount(
          gsl::narrow<size_t>(Configuration::instance().getIntValue(ConfigurationKey::HSM_MAX_SESSION_COUNT)))
    , mMaxUsedSessionCount(0)
    , mTimerManager(std::move(timerManager))
{
    Expect3(mHsmFactory != nullptr, "HsmFactory missing", std::logic_error);
    Expect3(mTimerManager != nullptr, "TimerManager missing", std::logic_error);

    TerminationHandler::instance().registerCallback([this](const bool hasError) {
        releasePool();

        if (hasError)
        {
            if (mActiveSessionCount > 0)
                std::cerr << "there are still " << mActiveSessionCount << " active HSM sessions" << std::endl;
        }
        else
        {
            Expect(mActiveSessionCount == 0, "there are still active HSM sessions");
        }
    });

    mTokenUpdater = teeTokenUpdaterFactory(*this, mTimerManager);
    mTokenUpdater->update();

    // Use the half of the configured HSM keep alive interval (Shannon) to handle aliasing effects.
    const auto keepAliveJobInterval = mHsmIdleTimeout / 4;
    mKeepAliveUpdateToken = mTimerManager->runRepeating(keepAliveJobInterval, keepAliveJobInterval, [this] {
        keepAvailableHsmSessionsAlive();
    });

    TVLOG(1) << "initialized HSM pool that supports up to " << mMaxSessionCount << " sessions";
    TVLOG(1) << "created HSM pool at " << (void*) this;
}


HsmPool::~HsmPool()
{
    releasePool();
}


void HsmPool::releasePool(void) noexcept
{
    mSessionRemover->notifyPoolRelease();// needs to be called outside lock

    std::lock_guard lock(mMutex);
    mTimerManager->cancel(mKeepAliveUpdateToken);
    mKeepAliveUpdateToken = Timer::NotAJob;

    mIsPoolReleased = true;

    // Release all inactive sessions.
    // To do this we just have to destroy them.
    // And to do that we just have to clear the container that, well, contains them.
    // And to do that we swap the container with a local instance (there is no clear() in a stack).
    decltype(mAvailableHsmSessions) sessions;
    sessions.swap(mAvailableHsmSessions);
}


HsmPoolSession HsmPool::acquire(void)
{
    std::unique_lock lock(mMutex);

    Expect(! mIsPoolReleased, "HsmPool::acquire called after the pool was released");

    if (mAvailableHsmSessions.empty())
    {
        if (mActiveSessionCount < mMaxSessionCount)
        {
            // Depending on how long it will take in real live to make a new connection, we may
            // have to release the mutex until the session object is ours.
            mAvailableHsmSessions.emplace_back(mHsmFactory->connect());
        }
        else
        {
            // We can not create another session.
            // Wait until one of the currently active sessions becomes available.
            mWaitForAvailableSessionVariable.wait(lock, [this] {
                return ! mAvailableHsmSessions.empty();
            });
        }
    }

    return activateSession();
}


HsmPoolSession HsmPool::activateSession(void)
{
    // Assume that the mutex is locked.
    Expect(! mAvailableHsmSessions.empty(), "there is no available HSM session");

    auto session = HsmPoolSession(std::move(mAvailableHsmSessions.front()), mSessionRemover);
    mAvailableHsmSessions.pop_front();
    ++mActiveSessionCount;
    if (mActiveSessionCount > mMaxUsedSessionCount)
        mMaxUsedSessionCount = mActiveSessionCount;

    // Furnish the session with an up-to-date tee token.
    session.session().setTeeToken(mTeeToken);

    return session;
}


void HsmPool::release(std::unique_ptr<HsmSession>&& session)
{
    {
        std::lock_guard lock(mMutex);

        --mActiveSessionCount;

        if (mIsPoolReleased)
        {
            // Do nothing. Ownership of `session` has been transferred to us.
            // As soon as we exit from the current block (this method), `session`
            // is destroyed and the HSM session is closed.
            return;
        }
        else
        {
            // This session may have been missed by the last call to keepAvailableHsmSessionsAlive.
            // In the unlikely case that the temporary owner did not make a call to the HSM via this session
            // object, we have to make sure that it is kept alive.
            keepHsmSessionAlive(*session);
            mAvailableHsmSessions.emplace_back(std::move(session));
        }
    }

    // There may be threads already waiting for an available session.
    mWaitForAvailableSessionVariable.notify_all();
}


size_t HsmPool::activeSessionCount(void) const
{
    std::lock_guard lock(mMutex);
    return mActiveSessionCount;
}


size_t HsmPool::inactiveSessionCount(void) const
{
    std::lock_guard lock(mMutex);
    return mAvailableHsmSessions.size();
}


size_t HsmPool::maxUsedSessionCount(void) const
{
    std::lock_guard lock(mMutex);
    return mMaxUsedSessionCount;
}


void HsmPool::resetMaxUsedSessionCount(void)
{
    std::lock_guard lock(mMutex);
    mMaxUsedSessionCount = 0;
}


void HsmPool::setTeeToken(ErpBlob&& teeToken)
{
    std::lock_guard lock(mMutex);
    mTeeToken = std::move(teeToken);
    TVLOG(1) << "HsmPool::setTeeToken, finished getting TEE token";
    TVLOG(1) << "got new tee token of size " << mTeeToken.data.size() << " with generation " << mTeeToken.generation;
}


const TeeTokenUpdater& HsmPool::getTokenUpdater() const
{
    return *mTokenUpdater;
}

TeeTokenUpdater& HsmPool::getTokenUpdater()
{
    return *mTokenUpdater;
}

bool HsmPool::isKeepAliveJobRunning(void) const
{
    std::lock_guard lock(mMutex);
    return mKeepAliveUpdateToken != Timer::NotAJob;
}


void HsmPool::keepAvailableHsmSessionsAlive(void)
{
    TVLOG(2) << "keeping hsm sessions alive at " << (void*) this;

    std::lock_guard lock(mMutex);

    for (auto& session : mAvailableHsmSessions)
    {
        if (session)
            keepHsmSessionAlive(*session);
    }
}


void HsmPool::keepHsmSessionAlive(HsmSession& session)
{
    /*
     * Rationale for the selection of threshold and rate of idle time reset (for available sessions).
     * The threshold is currently as 0.5 of the idle time out and the rate of update is 0.25 of the idle time out.
     * The idea is that there is one combination of the two which should be fairly stable. The math is that
     * at the Nth update the offset should be distinguishable from the last update and that the last update must be before
     * the idle time out happens.
     *
     * With
     *     T - idle timeout (15 minutes)
     *     N - index of the update (attempted reset of the idle timeout)
     *     O - offset of the threshold to "now"
     *     U - time between updates
     * this translates into
     *     0 < N * U - O < T
     * with healthy margins for the "<" comparisons.
     *
     * With O = T/2 and U = T/4 we find N=3 such that
     *     0 < 3 * T/4 - T/2 < T
     * which is equivalent to
     *     0 < T/4 < T
     * which for T=15 minutes is
     *     0 < 3.25 minutes < 15 minutes
     * which should be robust enough to handle any glitches of the timer.
     */
    const auto threshold = std::chrono::system_clock::now() - mHsmIdleTimeout / 2;
    session.keepAlive(threshold);
}


HsmFactory& HsmPool::getHsmFactory()
{
    return *mHsmFactory;
}
