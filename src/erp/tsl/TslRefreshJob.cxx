#include "erp/tsl/TslRefreshJob.hxx"

#include "erp/tsl/error/TslError.hxx"
#include "erp/util/Expect.hxx"


TslRefreshJob::TslRefreshJob(
    const std::shared_ptr<TslManager>& tslManager,
    const std::chrono::steady_clock::duration interval)
        : TimerJobBase("TslRefreshJob", interval)
        , mTslManager(tslManager)
        , mFirstIterationDone(false)
{
    Expect(tslManager != nullptr, "The refresh job must be initialized with TslManager");
}


void TslRefreshJob::onStart()
{
}


void TslRefreshJob::executeJob()
{
    // if refresh job fails it is no big problem as long as the available TrustStore-Information is actual,
    // the errors must be logged but no other reaction is necessary.
    // if refresh job never succeeds, the TslManager will try to do Update on demand as last resort,
    // and if it fails, then the related action will fail as well
    try
    {
        if (mFirstIterationDone)
        {
            VLOG(2) << "Executing TSL refresh job";
            mTslManager->updateTrustStoresOnDemand();
        }
        else
        {
            // ignore the first iteration that happens immediately
            mFirstIterationDone = true;
        }
    }
    catch(const TslError& e)
    {
        LOG(ERROR) << "Can not update TslManager, TslError: " << e.what();
    }
    catch(const std::runtime_error& e)
    {
        LOG(ERROR) << "Can not update TslManager, unexpected runtime exception: " << e.what();
    }
    catch(const std::logic_error& e)
    {
        LOG(ERROR) << "Can not update TslManager, unexpected logic exception: " << e.what();
    }
    catch(...)
    {
        LOG(ERROR) << "Can not update TslManager, unknown exception";
        throw;
    }
}


void TslRefreshJob::onFinish()
{
}
