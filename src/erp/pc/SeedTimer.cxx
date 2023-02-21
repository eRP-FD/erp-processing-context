/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/pc/SeedTimer.hxx"
#include "erp/util/TLog.hxx"


SeedTimerHandler::SeedTimerHandler(ThreadPool& pool, HsmPool& hsmPool,
                     std::chrono::steady_clock::duration interval,
                     SeedTimerHandler::AddEntropyFunction addEntropy)
    : FixedIntervalHandler{interval}
    , mThreadPool(pool)
    , mSeeder(hsmPool)
    , mAddEntropy(std::move(addEntropy))
    , mLastUpdate(decltype(mLastUpdate)::value_type())
{
}

void SeedTimerHandler::seedThisThread()
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

void SeedTimerHandler::timerHandler()
{
    refreshSeeds();
}


void SeedTimerHandler::refreshSeeds()
{
    TVLOG(1) << "Refreshing random seeds.";
    mLastUpdate = std::chrono::system_clock::now();
    mThreadPool.runOnAllThreads(
        [this]{seedThisThread();});
}

void SeedTimerHandler::healthCheck() const
{
    if (mLastUpdate.load() == decltype(mLastUpdate)::value_type())
    {
        Fail2("never updated", std::runtime_error);
    }
    const auto now = std::chrono::system_clock::now();
    if (mLastUpdate.load() + nextInterval().value() * 1.5 < now)
    {
        Fail2("last update is too old", std::runtime_error);
    }
}
