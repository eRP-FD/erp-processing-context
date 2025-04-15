/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/tsl/TslRefreshJob.hxx"

#include "shared/tsl/error/TslError.hxx"
#include "shared/util/Expect.hxx"


TslRefreshJob::TslRefreshJob(
    TslManager& tslManager,
    const std::chrono::steady_clock::duration interval)
        : TimerJobBase("TslRefreshJob", interval)
        , mTslManager(tslManager)
{
    TslRefreshJob::executeJob();
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
        TLOG(INFO) << "Executing TSL refresh job";
        mTslManager.updateTrustStoresOnDemand();
    }
    catch(const TslError& e)
    {
        TLOG(ERROR) << "Can not update TslManager, TslError: " << e.what();
    }
    catch(const std::runtime_error& e)
    {
        TLOG(ERROR) << "Can not update TslManager, unexpected runtime exception: " << e.what();
    }
    catch(const std::logic_error& e)
    {
        TLOG(ERROR) << "Can not update TslManager, unexpected logic exception: " << e.what();
    }
    catch(...)
    {
        TLOG(ERROR) << "Can not update TslManager, unknown exception";
        throw;
    }
}


void TslRefreshJob::onFinish()
{
}
