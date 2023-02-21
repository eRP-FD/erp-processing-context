/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SEEDTIMER_HXX
#define ERP_PROCESSING_CONTEXT_SEEDTIMER_HXX

#include "erp/crypto/SecureRandomGenerator.hxx"
#include "erp/crypto/Seeder.hxx"
#include "erp/util/PeriodicTimer.hxx"
#include "erp/server/ThreadPool.hxx"

class HsmPool;
class ThreadPool;


class SeedTimerHandler : public FixedIntervalHandler
{
public:
    using AddEntropyFunction = std::function<void (const SafeString&)>;
    explicit SeedTimerHandler(ThreadPool& pool,
                    HsmPool& hsmPool,
                    std::chrono::steady_clock::duration interval,
                    AddEntropyFunction addEntropy = &SecureRandomGenerator::addEntropy);

    void refreshSeeds();

    virtual void healthCheck() const;

private:
    void timerHandler() override;
    void seedThisThread();

    ThreadPool& mThreadPool;
    Seeder mSeeder;
    const AddEntropyFunction mAddEntropy;
    std::atomic<std::chrono::system_clock::time_point> mLastUpdate;
};

using SeedTimer = PeriodicTimer<SeedTimerHandler>;

#endif// ERP_PROCESSING_CONTEXT_SEEDTIMER_HXX
