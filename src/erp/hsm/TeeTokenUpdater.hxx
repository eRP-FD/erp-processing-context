/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_TEETOKENUPDATER_HXX
#define ERP_PROCESSING_CONTEXT_TEETOKENUPDATER_HXX

#include "erp/hsm/ErpTypes.hxx"

#include "erp/util/Timer.hxx"

#include <functional>


class HsmFactory;


/**
 * Update TeeTokens periodically in the background.
 * When a new token has been obtained, give it to the HsmPool, which will distribute it to its sessions.
 *
 * Uses two update times. One typically longer one between successful updates and a shorter one between failed retries.
 */
class TeeTokenUpdater
{
public:
    using TokenConsumer = std::function<void(ErpBlob&&)>;
    using TokenProvider = std::function<ErpBlob(HsmFactory&)>;

    explicit TeeTokenUpdater (
        TokenConsumer&& teeTokenConsumer,
        HsmFactory& hsmFactory,
        TokenProvider&& tokenProvider,
        std::shared_ptr<Timer> timerManager);
    ~TeeTokenUpdater (void);

    void update (void);
    void requestUpdate (void);
    void requestRetry (void);

    void healthCheck() const;

    // This factory is defined by the HsmPool.
    using TeeTokenUpdaterFactory = std::function<std::unique_ptr<TeeTokenUpdater>(
        std::function<void(ErpBlob&&)>, HsmFactory&, std::shared_ptr<Timer> timerManager)>;

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
        TokenConsumer&& teeTokenConsumer,
        HsmFactory& hsmFactory,
        TokenProvider&& tokenProvider,
        std::shared_ptr<Timer> timerManager,
        std::chrono::system_clock::duration updateInterval,
        std::chrono::system_clock::duration retryInterval);

private:
    const TokenConsumer mTeeTokenConsumer;
    HsmFactory& mHsmFactory;
    TokenProvider mTokenProvider;
    Timer::JobToken mUpdateJobToken;
    size_t mUpdateFailureCount;
    std::chrono::system_clock::duration mUpdateInterval;
    std::chrono::system_clock::duration mRetryInterval;
    std::atomic<std::chrono::system_clock::time_point> mLastUpdate;
    std::shared_ptr<Timer> mTimerManager;
};


#endif
