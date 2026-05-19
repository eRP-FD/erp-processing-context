#include "exporter/TRezeptEventProcessor.hxx"
#include "client/BfArMClient.hxx"
#include "client/FhirVZDClient.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "exporter/RunLoopScheduler.hxx"
#include "exporter/TRezeptTransformer.hxx"
#include "model/EventKvnr.hxx"
#include "pc/MedicationExporterServiceContext.hxx"
#include "shared/database/AccessTokenIdentity.hxx"
#include "shared/database/TransactionMode.hxx"
#include "shared/util/JsonLog.hxx"
#include "shared/util/TLog.hxx"
#include "util/RuntimeConfiguration.hxx"


using namespace std::chrono_literals;

namespace
{
const int BaseDelaySeconds = 60;
};

std::atomic<int> TRezeptEventProcessor::mRetryCount = 0;

template<typename FuncT>
decltype(auto) TRezeptEventProcessor::autocommit(FuncT&& function)
{
    return mServiceContext->transaction(TransactionMode::autocommit, std::forward<FuncT>(function));
}

TRezeptEventProcessor::TRezeptEventProcessor(const std::shared_ptr<MedicationExporterServiceContext>& serviceContext)
    : mJsonLog([this] {
        JsonLog jlog(LogId::INFO, JsonLog::makeInfoLogReceiver(), false);
        if (tlogContext.has_value())
        {
            jlog << KeyValue("x_request_id", *tlogContext);
        }
        jlog << KeyValue("x_context", mXContext);
        jlog << KeyValue("processor", "trezept");
        return jlog;
    })
    , mServiceContext(serviceContext)
    , mMaxRetries(Configuration::instance().getIntValue(ConfigurationKey::MEDICATION_EXPORTER_BFARM_CLIENT_NUM_RETRIES))
    , mLogContext(mXContext)
{
}

std::optional<model::Bundle> TRezeptEventProcessor::runFhirVzdSearch(const model::TRezeptEvent& event)
{
    const std::string clientId =
        Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_FHIR_VZD_CLIENT_ID);
    FhirVzdClient fhirVzdClient(clientId, mServiceContext->crlProvider(), event.getXContextId());
    fhirVzdClient.setEvent(&event);
    try
    {
        ModelExpect(event.getState() != model::TRezeptEvent::State::deadLetterQueue, "TRezeptEvent in invalid state.");
        A_27825.start("Start fhir vzd search with given org id");
        model::Bundle bundle = fhirVzdClient.performSearch(event.orgTelematikId().id());
        A_27825.finish();
        return bundle;
    }
    catch (const medication::exporter::exceptions::AuthenticationException& exc)
    {
        mJsonLog() << KeyValue("event", "FhirVzd Search Error")
                   << KeyValue("prescriptionId", event.getPrescriptionId().toString())
                   << KeyValue("reason", "Authentication error in runFhirVzdSearch.");
    }
    catch (const medication::exporter::exceptions::ServerErrorException& exc)
    {
        mJsonLog() << KeyValue("event", "FhirVzd Search Error")
                   << KeyValue("prescriptionId", event.getPrescriptionId().toString())
                   << KeyValue("reason", "Server error in runFhirVzdSearch.")
        << KeyValue("status",  toString(exc.statusCode()));
    }
    catch (const medication::exporter::exceptions::NetworkException& exc)
    {
        mJsonLog() << KeyValue("event", "FhirVzd Search Error")
                   << KeyValue("prescriptionId", event.getPrescriptionId().toString())
                   << KeyValue("reason", "Network error in runFhirVzdSearch.");
    }
    catch (const model::ModelException& exc)
    {
        mJsonLog() << KeyValue("event", "FhirVzd Search Error")
                   << KeyValue("prescriptionId", event.getPrescriptionId().toString())
                   << KeyValue("reason", "Model error in runFhirVzdSearch.");
    }
    catch (const ErpException& exc)
    {
        mJsonLog() << KeyValue("event", "FhirVzd Search Error")
                   << KeyValue("prescriptionId", event.getPrescriptionId().toString())
                   << KeyValue("reason", "Error in runFhirVzdSearch.");
    }
    catch (medication::exporter::exceptions::ClientException& exc)
    {
        mJsonLog() << KeyValue("event", "FhirVzd Search Error")
                   << KeyValue("prescriptionId", event.getPrescriptionId().toString())
                   << KeyValue("reason", "Client error in runFhirVzdSearch.");
    }
    return {};
}

std::optional<model::ErpTPrescriptionCarbonCopy>
TRezeptEventProcessor::createCarbonCopy(const model::TRezeptEvent& eventData, const model::Bundle& vzdSearchBundle)
{
    try
    {
        return TRezeptTransformer::transform(eventData.getKbvBundle(), eventData.getMedicationDispenseBundle(),
                                             eventData.getQesSigningTime(), eventData.getPrescriptionId(),
                                             eventData.orgTelematikId(), eventData.getJwtPharmacyOrganizationName(),
                                             vzdSearchBundle);
    }
    catch (const std::exception& exc)
    {
        mJsonLog() << KeyValue("event", "Carbon Copy Error")
                   << KeyValue("prescriptionId", eventData.getPrescriptionId().toString())
                   << KeyValue("reason", "Could not create carbon copy. ");
        mJsonLog().locationFromException(exc);
    }
    return {};
}

TRezeptEventProcessor::ResultType
TRezeptEventProcessor::handleOperationalBfarmError(const model::TRezeptEvent& event,
                                                   const medication::exporter::exceptions::OperationException& exc)
{
    // Generic log with http status:
    mJsonLog() << KeyValue("event", "sendCarbonCopy")
               << KeyValue("prescription_id", event.getPrescriptionId().toString())
               << KeyValue("reason",
                           std::string{"Server error (operational), http status: "} + toString(exc.statusCode()));

    if (exc.statusCode() == HttpStatus::Unauthorized)
    {
        // 401
        return TRezeptEventProcessor::ResultType::Retry;
    }
    if (exc.statusCode() == HttpStatus::TooManyRequests)
    {
        // 429
        return TRezeptEventProcessor::ResultType::Retry;
    }
    if (exc.statusCode() == HttpStatus::BadRequest || exc.statusCode() == HttpStatus::UnprocessableEntity)
    {
        // 400, 422
        // Additional logging info:
        A_27831.start("Log detailed server error.");
        mJsonLog() << KeyValue("event", "sendCarbonCopy")
                   << KeyValue("prescription_id", event.getPrescriptionId().toString())
                   << KeyValue("reason", "Bad Request or unprocessable entity") << KeyValue("details", exc.what());
        A_27831.finish();

        // deadletter
        return TRezeptEventProcessor::ResultType::Failure;
    }

    // For all 4xx http status codes:
    if (toNumericalValue(exc.statusCode()) >= toNumericalValue(HttpStatus::BadRequest) &&
        toNumericalValue(exc.statusCode()) <= toNumericalValue(HttpStatus::ClientClosedRequest))
    {
        // deadletter
        return TRezeptEventProcessor::ResultType::Failure;
    }

    // For all 5xx http status codes:
    if (toNumericalValue(exc.statusCode()) >= toNumericalValue(HttpStatus::InternalServerError) &&
        toNumericalValue(exc.statusCode()) <= toNumericalValue(HttpStatus::NetworkConnectTimeoutError))
    {
        return TRezeptEventProcessor::ResultType::Retry;
    }

    return TRezeptEventProcessor::ResultType::Retry;
}

TRezeptEventProcessor::ResultType TRezeptEventProcessor::sendCarbonCopy(const model::TRezeptEvent& event, const model::ErpTPrescriptionCarbonCopy& cc)
{
    const std::string clientId =
        Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_BFARM_CLIENT_ID);
    TVLOG(1) << "Sending carbon copy, clientId=" << clientId;
    BfArMClient client(clientId, mServiceContext->crlProvider(), event.getXContextId());
    client.setEvent(&event);
    A_27830.start("Test for retry / dead letter or success");
    try
    {
        client.sendCarbonCopy(cc);
        return TRezeptEventProcessor::ResultType::Success;
    }
    catch (const medication::exporter::exceptions::AuthenticationException& exc)
    {
        // 401
        mJsonLog() << KeyValue("event", "sendCarbonCopy")
                   << KeyValue("prescription_id", event.getPrescriptionId().toString())
                   << KeyValue("reason", "Authentication error.");
        return TRezeptEventProcessor::ResultType::Retry;
    }
    catch (const medication::exporter::exceptions::TokenAcquisitionException& exc)
    {
        mJsonLog() << KeyValue("event", "sendCarbonCopy")
                   << KeyValue("prescription_id", event.getPrescriptionId().toString())
                   << KeyValue("reason", std::string{"Server error (token), http status: "} + toString(exc.statusCode()));
        return TRezeptEventProcessor::ResultType::Retry;
    }
    catch (const medication::exporter::exceptions::OperationException& exc)
    {
        return handleOperationalBfarmError(event, exc);
    }
    catch (const medication::exporter::exceptions::ServerErrorException& exc)
    {
        // Avoid silent errors:
        mJsonLog() << KeyValue("event", "sendCarbonCopy")
                   << KeyValue("prescription_id", event.getPrescriptionId().toString())
                   << KeyValue("reason", std::string{"Server error, http status: "} + toString(exc.statusCode()));
    }
    catch (const medication::exporter::exceptions::NetworkException& exc)
    {
        mJsonLog() << KeyValue("event", "sendCarbonCopy")
                   << KeyValue("prescription_id", event.getPrescriptionId().toString())
                   << KeyValue("reason", "Network error.");
    }
    catch (const model::ModelException& exc)
    {
        mJsonLog() << KeyValue("event", "sendCarbonCopy")
                   << KeyValue("prescription_id", event.getPrescriptionId().toString())
                   << KeyValue("reason", "Model (data) error.");
    }
    catch (const ErpException& exc)
    {
        mJsonLog() << KeyValue("event", "sendCarbonCopy")
                   << KeyValue("prescription_id", event.getPrescriptionId().toString())
                   << KeyValue("reason", exc.what());
    }
    catch (const medication::exporter::exceptions::ClientException& exc)
    {
        mJsonLog() << KeyValue("event", "sendCarbonCopy")
                   << KeyValue("prescription_id", event.getPrescriptionId().toString())
                   << KeyValue("reason", "Client error.");
    }

    A_27830.finish();
    return TRezeptEventProcessor::ResultType::Retry;
}

TRezeptEventProcessor::ResultType TRezeptEventProcessor::process()
{
    try
    {
        return doProcess();
    }
    catch (const std::exception& e)
    {
        std::string reason = dynamic_cast<const model::ModelException*>(&e) ? "not given" : e.what();
        mJsonLog()
            .keyValue("event", "Processing T-Rezept events: Unexpected error. Wait before trying next processing.")
            .keyValue("reason", reason)
            .locationFromException(e);
        // Systemic error (like 'db schema wrong' or no db connection). Wait in order to reduce periodic error logs:
    }
    return ResultType::FailureOther;
}

TRezeptEventProcessor::ResultType TRezeptEventProcessor::doProcess()
{
    auto event = autocommit([&](auto& db) {
        const auto durationConsumerGuard =
            DurationConsumerGuard{tlogContext.value_or("none"),
                                  mServiceContext->getRuntimeConfigurationGetter()->getMetricsLogThresholdsMs()}
                .withContextId(mXContext);
        return db.processNextTRezeptEvent();
    });

    if (not event.has_value() || !event.value()) // optional<unique_ptr<>>
    {
        return TRezeptEventProcessor::ResultType::Idle;
    }

    event.value()->setXContextId(xContext());
    const model::TRezeptEvent& eventData = *event.value();
    const ScopedLogContext scopedLogContext{eventData.getXRequestId()};
    const auto durationConsumerGuard =
        DurationConsumerGuard{eventData.getXRequestId(),
                              mServiceContext->getRuntimeConfigurationGetter()->getMetricsLogThresholdsMs()}
            .withContextId(mXContext);

    mJsonLog() << KeyValue("event", "Start processing T-Rezept")
               << KeyValue("prescription_id", eventData.getPrescriptionId().toString())
               << KeyValue("retries", std::to_string(eventData.getRetryCount()));
    if (eventData.getRetryCount() > mMaxRetries)
    {
        markDeadLetterTRezeptEvent(eventData);
        return TRezeptEventProcessor::ResultType::Success;
    }

    A_27825.start("Start fhir vzd search");
    const auto vzdSearchBundle = runFhirVzdSearch(eventData);
    A_27825.finish();
    if (not vzdSearchBundle.has_value())
    {
        // Possible error logged (runFhirVzdSearch), retry.
        // NOTE: clarify if DLQ is better option.
        scheduleRetryTRezeptEvent(eventData);
        return TRezeptEventProcessor::ResultType::Retry;
    }

    A_27826_01.start("Create carbon copy");
    const auto cc = createCarbonCopy(eventData, vzdSearchBundle.value());
    A_27826_01.finish();

    A_27828.start("Transfer carbon copy to bfarm");
    A_27830.start("Test for retry / dead letter or success");
    const auto resultType = cc.has_value() ? sendCarbonCopy(eventData, cc.value()) : TRezeptEventProcessor::ResultType::Failure;
    switch (resultType)
    {
        case TRezeptEventProcessor::ResultType::Success:
            deleteTRezeptEvent(eventData);
            break;

        case TRezeptEventProcessor::ResultType::Retry:
            scheduleRetryTRezeptEvent(eventData);
            break;

        case TRezeptEventProcessor::ResultType::Failure:
            markDeadLetterTRezeptEvent(eventData);
            break;

        case TRezeptEventProcessor::ResultType::Idle:
        case TRezeptEventProcessor::ResultType::FailureOther:
            // error: enumeration value '...' not handled in switch [-Werror=switch]
            break;
    }
    A_27830.finish();
    A_27828.finish();

    return resultType;
}

void TRezeptEventProcessor::deleteTRezeptEvent(const model::TRezeptEvent& eventData)
{
    mJsonLog() << KeyValue("event", "Delete T-Rezept Event")
               << KeyValue("prescription_id", eventData.getPrescriptionId().toString());
    autocommit([&](auto& db) {
        db.deleteTRezeptEvent(eventData.getId());
    });
}

void TRezeptEventProcessor::scheduleRetryTRezeptEvent(const model::TRezeptEvent& eventData)
{
    A_27830.start("Calculate exponential backoff, first backoff is always the base delay.");
    auto retry = eventData.getRetryCount();
    if (retry < 0)
    {
        TVLOG(1) << "Unexpected negative retry value, reset to 0";
        retry = 0;
    }

    retry++;

    std::chrono::seconds retryDelaySeconds(BaseDelaySeconds);
    mJsonLog() << KeyValue("event", "Schedule T-Rezept Event")
               << KeyValue("prescription_id", eventData.getPrescriptionId().toString())
               << KeyValue("retryCount", std::to_string(mRetryCount.load()));
    autocommit([&](auto& db) {
        db.updateProcessingDelay(retry, retryDelaySeconds, eventData);
    });
    A_27830.finish();
}

void TRezeptEventProcessor::markDeadLetterTRezeptEvent(const model::TRezeptEvent& eventData)
{
    mJsonLog() << KeyValue("event", "DLQ T-Rezept Event")
               << KeyValue("prescription_id", eventData.getPrescriptionId().toString());
    autocommit([&](auto& db) {
        db.markDeadLetter(eventData);
    });
}

std::chrono::seconds TRezeptEventProcessor::getExponentialBackoffTime()
{
    static const int maxExpBackoff = Configuration::instance().getIntValue(
        ConfigurationKey::MEDICATION_EXPORTER_BFARM_CLIENT_MAX_EXPONENTIAL_BACKOFF);

    int cappedExpBackoff = std::min(mRetryCount.load(), maxExpBackoff);

    return std::chrono::seconds(BaseDelaySeconds * (1 << std::max(cappedExpBackoff, 0)));
}

boost::asio::awaitable<void>
TRezeptEventProcessor::runloopWorker(RunLoopScheduler& scheduler,
                                     std::weak_ptr<MedicationExporterServiceContext> serviceContext)
{
    A_27824.start("Run an asynch task for processing events");
    boost::asio::steady_timer timer{scheduler.getThreadPool().ioContext()};
    while (! scheduler.getThreadPool().ioContext().stopped())
    {
        auto serviceCtx = serviceContext.lock();
        if (! serviceCtx)
        {
            TVLOG(1) << "TRezeptEventProcessor::spawn no ctx";
            co_return;
        }
        // GEMREQ-start A_27859
        if (scheduler.checkIsPaused(serviceCtx, exporter::RuntimeConfiguration::ProcessorType::T_REZEPT))
        {
            serviceCtx.reset();
            timer.expires_after(100ms);
            co_await timer.async_wait(boost::asio::as_tuple(boost::asio::deferred));
            continue;
        }
        // GEMREQ-end A_27859

        TRezeptEventProcessor::ResultType result = TRezeptEventProcessor{serviceCtx}.process();
        switch (result)
        {
            case TRezeptEventProcessor::ResultType::Idle:
                // Let the loop breathe for a short time, otherwise in the rare case when no
                // more tasks are to be processed, it would permanently query the database.
                timer.expires_after(std::chrono::seconds(2));
                co_await timer.async_wait(boost::asio::as_tuple(boost::asio::deferred));
                break;
            case TRezeptEventProcessor::ResultType::Success:
                // Event was properly handled, continue with next event, reset exponential backoff.
                TRezeptEventProcessor::mRetryCount.store(0);
                co_await async_immediate(co_await boost::asio::this_coro::executor);
                break;
            case TRezeptEventProcessor::ResultType::Retry:
                timer.expires_after(getExponentialBackoffTime());
                co_await timer.async_wait(boost::asio::as_tuple(boost::asio::deferred));
                mRetryCount.fetch_add(1);
                break;
            case TRezeptEventProcessor::ResultType::Failure:
                // Event put to DLQ, continue with next event.
                co_await async_immediate(co_await boost::asio::this_coro::executor);
                break;
            case TRezeptEventProcessor::ResultType::FailureOther:
                timer.expires_after(std::chrono::seconds(BaseDelaySeconds));
                co_await timer.async_wait(boost::asio::as_tuple(boost::asio::deferred));
                break;
        }
    }
    A_27824.finish();
}

const std::string& TRezeptEventProcessor::xContext() const
{
    return mXContext;
}
