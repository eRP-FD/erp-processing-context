/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/pc/SeedTimer.hxx"
#include "erp/util/TLog.hxx"


SeedTimer::SeedTimer(ThreadPool& pool, HsmPool& hsmPool,
                     std::chrono::steady_clock::duration interval,
                     SeedTimer::AddEntropyFunction addEntropy)
    : PeriodicTimer(interval)
    , mThreadPool(pool)
    , mSeeder(hsmPool)
    , mAddEntropy(std::move(addEntropy))
    , mInterval(interval)
    , mLastUpdate(decltype(mLastUpdate)::value_type())
{
}

void SeedTimer::seedThisThread()
{
    try
    {
        mAddEntropy(mSeeder.getNextSeed());
    }
    catch (const std::exception& ex)
    {
        TLOG(ERROR) << "exception during refresh of random seeds: " << ex.what();
        throw;
    }
    catch (...)
    {
        TLOG(ERROR) << "unknown exception during refresh of random seeds";
        throw;
    }
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
    if (mLastUpdate.load() == decltype(mLastUpdate)::value_type())
    {
        throw std::runtime_error("never updated");
    }
    const auto now = std::chrono::system_clock::now();
    if (mLastUpdate.load() + mInterval * 1.5 < now)
    {
        throw std::runtime_error("last update is too old");
    }
}
