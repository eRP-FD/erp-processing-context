/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "PushEventCreator.hxx"

#include "erp/database/push/PushEventDataCollector.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Demangle.hxx"
#include "shared/util/JsonLog.hxx"

namespace
{
void createPushEvent(const PushEventDataCollector& pushEventData, PcServiceContext& serviceContext)
{
    try
    {
        const auto dt = DurationConsumer::getCurrent().getTimer(DurationCategory::pusheventcreation, "inside-callback");
        const bool isRegistered = serviceContext.readOnlyPushDatabaseFactory()->isPushRegistered(
            value(pushEventData.hashedKvnr()), *pushEventData.channelId());

        if (isRegistered)
        {
            serviceContext.pushExporterDatabaseFactory()->createPushEvent(pushEventData);
        }
    }
    catch (const std::exception& ex)
    {
        std::ostringstream msg;
        JsonLog log{LogId::INFO, [](std::string&& msg) {
                        TLOG(WARNING) << "An exception was thrown, when creating a push event: " << msg;
                    }};
        log.locationFromException(ex);
        log.keyValue("type", util::demangle(typeid(ex).name()));
        if (const auto* erpEx = dynamic_cast<const ErpException*>(&ex))
        {
            log.keyValue("reason", erpEx->what());
#ifdef ENABLE_DEBUG_LOG
            if (const auto& diag = erpEx->diagnostics())
            {
                log.details(*diag);
            }
#endif
        }
    }
    catch (...)
    {
        TLOG(WARNING) << "An unknown object was thrown, when creating a push event.";
    }
}
}

PushEventCreator::PushEventCreator(boost::asio::io_context& ioContext)
    : mStrand{boost::asio::make_strand(ioContext)}
{
}


void PushEventCreator::tryPostCreatePushEvent(PushEventDataCollector pushEventData, PcServiceContext& serviceContext)
{
    using namespace std::chrono_literals;
    const auto& config = Configuration::instance();
    const auto maxdepth = PushEventCreator::maxdepth(config);
    std::unique_lock guard{mQueueMutex};
    if (mQueueDepth >= maxdepth)
    {
        ++mDiscarded;
        auto now = std::chrono::steady_clock::now();
        if (mLastWarning + warnInterval(config) <= now)
        {
            mLastWarning = now;
            TLOG(WARNING) << fmt::format(
                "Push event queue max depth reached ({}) events discarded since last warning: {}", maxdepth,
                mDiscarded);
            mDiscarded = 0;
        }
        return;
    }
    ++mQueueDepth;
    guard.unlock();
    auto fun = std::bind(&createPushEvent, std::move(pushEventData), std::ref(serviceContext));
    post(std::move(fun), mStrand, [this] {
        std::lock_guard guard{mQueueMutex};
        --mQueueDepth;
    });
}

size_t PushEventCreator::maxdepth(const Configuration& config)
{
    return gsl::narrow<size_t>(config.getIntValue(ConfigurationKey::PUSH_EVENT_MAX_QUEUE));
}

std::chrono::seconds PushEventCreator::warnInterval(const Configuration& config)
{
    return std::chrono::seconds{config.getIntValue(ConfigurationKey::PUSH_EVENT_DROPPED_WARN_INTERVAL_SEC)};
}

size_t PushEventCreator::currentDepth() const
{
    std::unique_lock guard{mQueueMutex};
    return mQueueDepth;
}

size_t PushEventCreator::discardedSinceWarn() const
{
    std::unique_lock guard{mQueueMutex};
    return mDiscarded;
}
