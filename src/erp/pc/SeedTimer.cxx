#include "erp/pc/SeedTimer.hxx"

#include "erp/util/TLog.hxx"


SeedTimer::SeedTimer(ThreadPool& pool, HsmPool& hsmPool, size_t threadCount,
                     std::chrono::steady_clock::duration interval,
                     SeedTimer::AddEntropyFunction addEntropy)
    : PeriodicTimer(interval)
    , mThreadPool(pool)
    , mThreadCount(threadCount)
    , mSeeder(hsmPool)
    , mAddEntropy(std::move(addEntropy))
    , mInterval(interval)
{
}

void SeedTimer::seedThisThread()
{
    mAddEntropy(mSeeder.getNextSeed());
}

void SeedTimer::timerHandler()
{
    refreshSeeds();
}

void SeedTimer::refreshSeeds()
{
    TVLOG(1) << "Refreshing random seeds.";
    mLastUpdate = std::chrono::system_clock::now();
    mThreadPool.runOnAllThreads(
        [this]{seedThisThread();});
}

void SeedTimer::healthCheck() const
{
    if (mLastUpdate == decltype(mLastUpdate)())
    {
        throw std::runtime_error("never updated");
    }
    const auto now = std::chrono::system_clock::now();
    if (mLastUpdate + mInterval * 1.5 < now)
    {
        throw std::runtime_error("last update is too old");
    }
}
