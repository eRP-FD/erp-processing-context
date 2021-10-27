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


class SeedTimer
    : public PeriodicTimer
{
public:
    using AddEntropyFunction = std::function<void (const SafeString&)>;
    explicit SeedTimer(ThreadPool& pool,
                    HsmPool& hsmPool,
                    size_t threadCount,
                    std::chrono::steady_clock::duration interval,
                    AddEntropyFunction addEntropy = &SecureRandomGenerator::addEntropy);

    void refreshSeeds();

    virtual void healthCheck() const;

private:
    void timerHandler() override;
    void seedThisThread();

    ThreadPool& mThreadPool;
    const size_t mThreadCount;
    Seeder mSeeder;
    const AddEntropyFunction mAddEntropy;
    std::chrono::steady_clock::duration mInterval;
    std::atomic<std::chrono::system_clock::time_point> mLastUpdate;
};



#endif// ERP_PROCESSING_CONTEXT_SEEDTIMER_HXX
