/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/hsm/TeeTokenUpdater.hxx"
#include "erp/hsm/ErpTypes.hxx"
#include "erp/hsm/production/TeeTokenProductionUpdater.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/JsonLog.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/TerminationHandler.hxx"


namespace
{
    std::string getExceptionMessage (void)
    {
        try
        {
            std::rethrow_exception(std::current_exception());
        }
        catch(const std::exception& e)
        {
            return e.what();
        }
        catch(...)
        {
            return "unknown";
        }
    }
}


TeeTokenUpdater::TeeTokenUpdater (
    std::function<void(ErpBlob&&)>&& teeTokenConsumer,
    HsmFactory& hsmFactory,
    TokenProvider&& tokenProvider,
    std::shared_ptr<Timer> timerManager)
    : TeeTokenUpdater(
          std::move(teeTokenConsumer),
          hsmFactory,
          std::move(tokenProvider),
          std::move(timerManager),
          std::chrono::seconds(Configuration::instance().getOptionalIntValue(ConfigurationKey::TEE_TOKEN_UPDATE_SECONDS, 1200)),
          std::chrono::seconds(Configuration::instance().getOptionalIntValue(ConfigurationKey::TEE_TOKEN_RETRY_SECONDS, 60)))
{
}


TeeTokenUpdater::TeeTokenUpdater (
    std::function<void(ErpBlob&&)>&& teeTokenConsumer,
    HsmFactory& hsmFactory,
    TokenProvider&& tokenProvider,
    std::shared_ptr<Timer> timerManager,
    std::chrono::system_clock::duration updateInterval,
    std::chrono::system_clock::duration retryInterval)
    : mTeeTokenConsumer(std::move(teeTokenConsumer)),
      mHsmFactory(hsmFactory),
      mTokenProvider(std::move(tokenProvider)),
      mUpdateJobToken(),
      mUpdateFailureCount(0),
      mUpdateInterval(updateInterval),
      mRetryInterval(retryInterval),
      mLastUpdate(decltype(mLastUpdate)::value_type()),
      mTimerManager(std::move(timerManager))
{
    Expect(mTokenProvider!=nullptr, "can not create TeeTokenUpdater without token provider");
    Expect3(mTimerManager!=nullptr, "TimerManager missing", std::logic_error);
    TVLOG(0) << "TeeTokenUpdater will update every " << std::chrono::duration_cast<std::chrono::seconds>(mUpdateInterval).count() << " seconds";
    TVLOG(0) << "TeeTokenUpdater will retry  every " << std::chrono::duration_cast<std::chrono::seconds>(mRetryInterval).count() << " seconds";
}


TeeTokenUpdater::~TeeTokenUpdater (void)
{
    mTimerManager->cancel(mUpdateJobToken);
}


void TeeTokenUpdater::update (void)
{
    try
    {
        mUpdateJobToken = Timer::NotAJob;

        mTeeTokenConsumer(mTokenProvider(mHsmFactory));

        // Update was successful.
        JsonLog(LogId::HSM_INFO, JsonLog::makeVLogReceiver(0))
                .message("tee token update successful")
                .keyValue("failed-update-count", std::to_string(mUpdateFailureCount));
        mUpdateFailureCount = 0;
        mLastUpdate = std::chrono::system_clock::now();

        requestUpdate();
    }
    catch(...)
    {
        TVLOG(0) << "update of Tee token failed";

        ++mUpdateFailureCount;

        JsonLog(LogId::HSM_WARNING, JsonLog::makeWarningLogReceiver())
            .message("tee token update failed, will try again")
            .details(getExceptionMessage())
            .keyValue("failed-update-count", std::to_string(mUpdateFailureCount));
        requestRetry();
    }
}


void TeeTokenUpdater::requestUpdate (void)
{
    mUpdateJobToken = mTimerManager->runIn(
        mUpdateInterval,
        [this]{update();});
}


void TeeTokenUpdater::requestRetry (void)
{
    mUpdateJobToken = mTimerManager->runIn(
        mRetryInterval,
        [this]{update();});
}


void TeeTokenUpdater::healthCheck() const
{
    if (mLastUpdate.load() == decltype(mLastUpdate)::value_type())
    {
        Fail2("never updated successfully", std::runtime_error);
    }
    const auto now = std::chrono::system_clock::now();
    if (mLastUpdate.load() + mUpdateInterval * 1.5 < now)
    {
        Fail2("last update is too old", std::runtime_error);
    }
}

TeeTokenUpdater::TeeTokenUpdaterFactory TeeTokenUpdater::createProductionTeeTokenUpdaterFactory (void)
{
#if WITH_HSM_TPM_PRODUCTION > 0
    return [](auto&& tokenConsumer, auto& hsmFactory, std::shared_ptr<Timer> timerManager)
    {
        return std::make_unique<TeeTokenUpdater>(
            std::forward<decltype(tokenConsumer)>(tokenConsumer),
            hsmFactory,
            [](HsmFactory& factory)
            {
                return TeeTokenProductionUpdater::provideTeeToken(factory);
            },
            timerManager);
    };
#else
    Fail2("production HSM/TPM not compiled in.", std::logic_error);
#endif
}

TeeTokenUpdater::TeeTokenUpdaterFactory TeeTokenUpdater::createMockTeeTokenUpdaterFactory (void)
{
    return [](auto&& tokenConsumer, auto& hsmFactory, std::shared_ptr<Timer> timerManager)
    {
        return std::make_unique<TeeTokenUpdater>(
            std::forward<decltype(tokenConsumer)>(tokenConsumer),
            hsmFactory,
            [](HsmFactory&)
            {
                // Tests that use a mock HSM don't need a TEE token. An empty blob is enough.
                return ErpBlob();
            },
            timerManager);
    };
}
