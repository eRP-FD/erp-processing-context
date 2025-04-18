/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEETOKENUPDATER_HXX
#define ERP_PROCESSING_CONTEXT_TEETOKENUPDATER_HXX

#include "shared/hsm/ErpTypes.hxx"

#include "shared/deprecated/Timer.hxx"

#include <functional>


class HsmFactory;
class HsmPool;


/**
 * Update TeeTokens periodically in the background.
 * When a new token has been obtained, give it to the HsmPool, which will distribute it to its sessions.
 *
 * Uses two update times. One typically longer one between successful updates and a shorter one between failed retries.
 */
class TeeTokenUpdater
{
public:
    using TokenRefresher = std::function<void(HsmPool&)>;

    explicit TeeTokenUpdater (
        HsmPool& hsmPool,
        TokenRefresher&& tokenRefresher,
        std::shared_ptr<Timer> timerManager);
    ~TeeTokenUpdater (void);

    void update (void);
    void requestUpdate (void);
    void requestRetry (void);

    void healthCheck() const;

    // This factory is defined by the HsmPool.
    using TeeTokenUpdaterFactory =
        std::function<std::unique_ptr<TeeTokenUpdater>(HsmPool&, std::shared_ptr<Timer> timerManager)>;

    // Convenience function to create a token updater that can be used in production.
    static TeeTokenUpdaterFactory createProductionTeeTokenUpdaterFactory (void);

    // Convenience function to create a token updater that returns an empty Tee token blob that can only be used
    // in tests.
    static TeeTokenUpdaterFactory createMockTeeTokenUpdaterFactory (void);


protected:
    /**
     * Custom constructor for tests that allows sub-second update intervals.
     */
    TeeTokenUpdater (
        HsmPool& hsmPool,
        TokenRefresher&& tokenRefresher,
        std::shared_ptr<Timer> timerManager,
        std::chrono::system_clock::duration updateInterval,
        std::chrono::system_clock::duration retryInterval);

private:
    HsmPool& mHsmPool;
    TokenRefresher mTokenRefresher;
    Timer::JobToken mUpdateJobToken;
    size_t mUpdateFailureCount;
    std::chrono::system_clock::duration mUpdateInterval;
    std::chrono::system_clock::duration mRetryInterval;
    std::atomic<std::chrono::system_clock::time_point> mLastUpdate;
    std::shared_ptr<Timer> mTimerManager;
};


#endif
