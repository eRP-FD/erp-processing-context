/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "exporter/eventprocessing/push/PushEventProcessor.hxx"
#include "PushEventEncoder.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "exporter/RunLoopScheduler.hxx"
#include "exporter/client/push/PushGatewayClient.hxx"
#include "exporter/model/DeviceId.hxx"
#include "exporter/model/push/BatchResponse.hxx"
#include "exporter/model/push/EncryptedNotification.hxx"
#include "exporter/model/push/ErrorResponse.hxx"
#include "exporter/model/push/PushNotificationContext.hxx"
#include "exporter/model/push/PushNotificationEvent.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "shared/audit/AuditDataCollector.hxx"
#include "shared/beast/BoostBeastStringWriter.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/DurationConsumer.hxx"
#include "shared/util/Hash.hxx"
#include "shared/util/TLog.hxx"

#include <utility>

boost::asio::awaitable<void>
PushEventProcessor::runloopWorker(RunLoopScheduler& scheduler,
                                  std::weak_ptr<MedicationExporterServiceContext> serviceContext)
{
    using namespace std::chrono_literals;
    boost::asio::steady_timer timer{scheduler.getThreadPool().ioContext()};
    while (! scheduler.getThreadPool().ioContext().stopped())
    {
        auto serviceCtx = serviceContext.lock();
        if (! serviceCtx)
        {
            TVLOG(1) << "PushEventProcessor::runloopWorker no ctx";
            co_return;
        }
        if (scheduler.checkIsPaused(serviceCtx, exporter::RuntimeConfiguration::ProcessorType::PUSH))
        {
            serviceCtx.reset();
            timer.expires_after(100ms);
            co_await timer.async_wait(boost::asio::as_tuple(boost::asio::deferred));
            continue;
        }

        auto client = PushGatewayClient::create(serviceCtx->crlProvider());
        const auto result = PushEventProcessor{serviceCtx, client.get()}.process();
        switch (result)
        {

            case ResultType::Idle:
                timer.expires_after(15s);
                co_await timer.async_wait(boost::asio::as_tuple(boost::asio::deferred));
                break;
            case ResultType::Success:
            case ResultType::FailureRetry:
            case ResultType::FailureRejected:
                co_await async_immediate(co_await boost::asio::this_coro::executor);
                break;
        }
    }
}

PushEventProcessor::PushEventProcessor(const std::shared_ptr<MedicationExporterServiceContext>& serviceContext,
                                       gsl::not_null<PushGatewayClient*> client)
    : mServiceContext(serviceContext)
    , mClient(std::move(client))
    , mJsonLogFactory([this] {
        JsonLog jlog(LogId::INFO, JsonLog::makeInfoLogReceiver(), false);
        if (tlogContext.has_value())
        {
            jlog << KeyValue("x_request_id", *tlogContext);
        }
        jlog << KeyValue("x_context", mXContext);
        jlog << KeyValue("processor", "notification");
        return jlog;
    })
    , mLogContext(mXContext)
{
}

PushEventProcessor::ResultType PushEventProcessor::process()
{
    auto durationConsumerGuard =
        DurationConsumerGuard{tlogContext.value_or("none"),
                              mServiceContext->getRuntimeConfigurationGetter()->getMetricsLogThresholdsMs()}
            .withContextId(mXContext);
    std::vector<model::PushNotificationContext> events;
    try
    {
        events = fetchNextEvent();
    }
    catch (const std::exception& ex)
    {
        auto jlog = mJsonLogFactory();
        jlog << KeyValue("event", "Push Notification") << KeyValue("what", ex.what());
        jlog << KeyValue("reason", "exception during event fetching from databases");
        return ResultType::FailureRetry;
    }
    if (! events.empty())
    {
        return processEvents(events);
    }
    return ResultType::Idle;
}

PushEventProcessor::ResultType
PushEventProcessor::processEvents(const std::vector<model::PushNotificationContext>& events)
{
    const auto& firstEvent = events.front();
    ResultType combinedResult = magic_enum::enum_values<ResultType>().back();
    A_27163.start("für jede App-Registrierung");
    for (const auto& event : events)
    {
        const ScopedLogContext scopedLogContextWithXRequestId{event.xRequestId()};
        try
        {
            auto currentResult = processEvent(event);
            // If one result is success the combined result shall be success, same for Retry.
            combinedResult = std::min(combinedResult, currentResult);
        }
        catch (const model::ModelException& me)
        {
            auto jlog = mJsonLogFactory();
            jlog << KeyValue("event", "Push Notification");
            jlog << KeyValue("reason", "ModelException during event processing");
            jlog << KeyValue(event.pushNotification().identifierType(), event.pushNotification().identifier());
        }
        catch (const std::exception& ex)
        {
            auto jlog = mJsonLogFactory();
            jlog << KeyValue("event", "Push Notification") << KeyValue("what", ex.what());
            jlog << KeyValue("reason", "exception during event processing");
            jlog << KeyValue(event.pushNotification().identifierType(), event.pushNotification().identifier());
        }
    }
    switch (combinedResult)
    {
        case ResultType::Success:
            deletePushNotification(firstEvent, "Deleting event after successful transfer");
            break;
        case ResultType::FailureRejected:
            deletePushNotification(firstEvent, "Deleting event because pushkey was rejected");
            break;
        case ResultType::Idle:
        case ResultType::FailureRetry: {
            using namespace std::chrono_literals;
            if (firstEvent.created() + 12h < model::Timestamp::now())
            {
                A_28473.start("Retry after 12h, delete if still failing");
                deletePushNotification(firstEvent, "Deleting push event older than 12 hours");
                return ResultType::FailureRetry;
            }
            if (firstEvent.retryCount() >=
                Configuration::instance().getIntValue(ConfigurationKey::MEDICATION_EXPORTER_PUSH_MAX_RETRY_COUNT))
            {
                deletePushNotification(firstEvent, "Deleting event after max retryCount");
            }
            else
            {
                auto delay = firstEvent.rescheduleDelay();
                mJsonLogFactory() << KeyValue("event", "Reschedule event after error")
                                  << KeyValue("channel_id", firstEvent.pushNotification().channelId())
                                  << KeyValue("prescription_id", firstEvent.pushNotification().identifier())
                                  << KeyValue("kvnr", firstEvent.hashedKvnr().getLoggingId())
                                  << KeyValue("retryCount", std::to_string(firstEvent.retryCount()));
                mServiceContext->transaction(TransactionMode::autocommit, [&](auto& db) {
                    db.updatePushProcessingDelay(firstEvent.retryCount() + 1, delay, firstEvent.hashedKvnr(),
                                                 firstEvent.eventId());
                });
            }
        }
        break;
    }
    return combinedResult;
}

PushEventProcessor::ResultType PushEventProcessor::processEvent(const model::PushNotificationContext& event)
{
    auto durationConsumerGuardWithXRequestId = DurationConsumerGuard{
        event.xRequestId(), mServiceContext->getRuntimeConfigurationGetter()->getMetricsLogThresholdsMs()};

    if (mServiceContext->isPushGatewayFailing(event.device().data().mUrl))
    {
        mJsonLogFactory() << KeyValue("event", "Reschedule event due to backoff of push gateway")
                          << KeyValue("channel_id", event.pushNotification().channelId())
                          << KeyValue("prescription_id", event.pushNotification().identifier())
                          << KeyValue("kvnr", event.hashedKvnr().getLoggingId())
                          << KeyValue("retryCount", std::to_string(event.retryCount()));
        return ResultType::FailureRetry;
    }

    A_27436.start("DARF NICHT Push Notifications erzeugen und ans Push Gateway übermitteln, die unverschlüsselte "
                  "personenbezogene Daten enthalten.");
    // GEMREQ-start A_27161#processEvent
    auto encryptedNotification = PushEventEncoder::encode(event);
    A_27652.start("sicherstellen, dass Push Notifications ausschließlich an die in der Registrierung der FdV-Instanz "
                  "hinterlegte URL (aus: data/url) übermittelt werden.");
    const auto start = model::Timestamp::now();
    // GEMREQ-start A_27652
    const auto response = mClient->sendPushNotification(encryptedNotification, event.device().data().mUrl);
    // GEMREQ-end A_27652
    // GEMREQ-end A_27161#processEvent
    return processResponse(event, response, model::Timestamp::now() - start, model::Timestamp::now() - event.created());
}

std::vector<model::PushNotificationContext> PushEventProcessor::fetchNextEvent()
{
    auto nextPushNotification = mServiceContext->transaction(TransactionMode::autocommit, [&](auto& db) {
        return db.processNextPushNotification();
    });
    if (nextPushNotification)
    {
        TVLOG(1) << "fetched: " << nextPushNotification->pushNotification().serializeToJsonString();
        mJsonLogFactory() << KeyValue("event", "Start Processing Push Event")
                          << KeyValue("channel_id", nextPushNotification->pushNotification().channelId())
                          << KeyValue("prescription_id", nextPushNotification->pushNotification().identifier())
                          << KeyValue("kvnr", nextPushNotification->hashedKvnr().toHex())
                          << KeyValue("retryCount", std::to_string(nextPushNotification->retryCount()));
        // GEMREQ-start A_27161#fetchNextEvent
        auto erpDatabase = mServiceContext->erpDatabaseFactory();
        A_27163.start("prüfen, ob für den Versicherten eine App-Registrierung existiert");
        auto pushNotificationContexts = erpDatabase->retrievePushNotificationEventData(*nextPushNotification);
        if (pushNotificationContexts.empty())
        {
            mJsonLogFactory() << KeyValue("event", "Deleting push event because no app-registration exists")
                              << KeyValue("channel_id", nextPushNotification->pushNotification().channelId())
                              << KeyValue("prescription_id", nextPushNotification->pushNotification().identifier())
                              << KeyValue("kvnr", nextPushNotification->hashedKvnr().toHex())
                              << KeyValue("retryCount", std::to_string(nextPushNotification->retryCount()))
                              << KeyValue("created", nextPushNotification->created().toXsDateTime());
            mServiceContext->transaction(TransactionMode::autocommit, [&](auto& db) {
                db.deletePushNotification(nextPushNotification->hashedKvnr(), nextPushNotification->id());
            });
            return {};
        }
        mJsonLogFactory() << KeyValue("event",
                                      fmt::format("Found {} app registrations", pushNotificationContexts.size()))
                          << KeyValue("channel_id", nextPushNotification->pushNotification().channelId())
                          << KeyValue("prescription_id", nextPushNotification->pushNotification().identifier())
                          << KeyValue("kvnr", nextPushNotification->hashedKvnr().toHex());
        // GEMREQ-start A_27160
        for (auto& pushNotificationContext : pushNotificationContexts)
        {
            if (PushEventEncoder::encryptionKeyNeedsUpdate(pushNotificationContext.encryptionKey()))
            {
                mJsonLogFactory()
                    << KeyValue("event", "Encryption key needs update")
                    << KeyValue("channel_id", nextPushNotification->pushNotification().channelId())
                    << KeyValue("prescription_id", nextPushNotification->pushNotification().identifier())
                    << KeyValue("kvnr", nextPushNotification->hashedKvnr().toHex())
                    << KeyValue("pushkey_hashed",
                                String::toHexString(Hash::sha256(pushNotificationContext.device().pushkey())))
                    << KeyValue("app_id_hashed",
                                String::toHexString(Hash::sha256(pushNotificationContext.device().appId())))
                    << KeyValue("time_key_created", pushNotificationContext.encryptionKey().monthInfoString());
                pushNotificationContext.setEncryptionKey(
                    PushEventEncoder::updateEncryptionKey(pushNotificationContext.encryptionKey()));
                erpDatabase->updateEncryptionKey(nextPushNotification->hashedKvnr(), pushNotificationContext);
            }
        }
        // GEMREQ-end A_27161#fetchNextEvent
        erpDatabase->commitTransaction();
        // GEMREQ-end A_27160

        return pushNotificationContexts;
    }
    return {};
}

PushEventProcessor::ResultType PushEventProcessor::processResponse(const model::PushNotificationContext& event,
                                                                   const ClientResponse& response,
                                                                   const std::chrono::milliseconds httpResponseTime,
                                                                   const std::chrono::milliseconds duration)
{
    TVLOG(1) << "PushEventProcessor::processResponse: "
             << BoostBeastStringWriter::serializeResponse(response.getHeader(), response.getBody());
    auto jlog = mJsonLogFactory();
    jlog << KeyValue("log_type", "bde") << KeyValue("timestamp", model::Timestamp::now().toXsDateTime())
         << KeyValue("request_operation", event.device().data().path() + "notifyEncrypted/batch")
         << KeyValue("operation", "ERP.UC_5_10") << KeyValue("endpoint_host", event.device().data().fqdn())
         << KeyValue("prescription_id", event.pushNotification().identifier())
         << KeyValue("kvnr", event.hashedKvnr().getLoggingId())
         << KeyValue("channel_id", event.pushNotification().channelId())
         << KeyValue("pushkey_hashed", String::toHexString(Hash::sha256(event.device().pushkey())))
         << KeyValue("app_id_hashed", String::toHexString(Hash::sha256(event.device().appId())));
    jlog.keyValue("response_code", toNumericalValue(response.getHeader().status()));
    jlog.keyValue("response_time", httpResponseTime.count());
    jlog.keyValue("duration_in_ms", duration.count());
    // expected return codes: 200,400,401,413,500,503
    // https://gematik.github.io/gem-push-notifications-concept/1.1/#push_gateway_openapi.html
    if (response.getHeader().status() != HttpStatus::OK)
    {
        try
        {
            auto errorResponse = model::ErrorResponse::parseResponse(response.getBody());
            jlog << KeyValue("error", errorResponse.getError());
        }
        catch (const std::exception& ex)
        {
            jlog << KeyValue("error", "exception during error response parsing");
            TLOG(INFO) << "response body: " << response.getBody();
        }
        if (is5xxInternalError(response.getHeader().status()))
        {
            mJsonLogFactory() << KeyValue("event", "5 minute backoff for push gateway")
                              << KeyValue("channel_id", event.pushNotification().channelId())
                              << KeyValue("prescription_id", event.pushNotification().identifier())
                              << KeyValue("kvnr", event.hashedKvnr().getLoggingId())
                              << KeyValue("endpoint_host", event.device().data().fqdn());
            mServiceContext->pushGatewayFailed(event.device().data().mUrl);
        }
        return ResultType::FailureRetry;
    }
    try
    {
        ResultType res = ResultType::Success;
        auto batchResponse = model::BatchResponse::parseResponse(response.getBody());
        if (! batchResponse.getResults().empty())
        {
            const auto& firstResult = batchResponse.getResults().front();
            jlog << KeyValue("status", magic_enum::enum_name(firstResult.status));
            if (firstResult.error.has_value())
            {
                jlog << KeyValue("error", *firstResult.error);
            }
            if (firstResult.status == model::BatchResponse::Status::failed)
            {
                res = ResultType::FailureRetry;
            }
            jlog << KeyValue("rejected_push_key", (firstResult.rejected.empty() ? "false" : "true"));
            if (processRejected(event, firstResult))
            {
                res = ResultType::FailureRejected;
            }
        }
        return res;
    }
    catch (const std::exception& ex)
    {
        jlog << KeyValue("error", "exception during batch response parsing / processing");
        TLOG(INFO) << "response body: " << response.getBody();
        TLOG(INFO) << "exception: " << ex.what();
    }
    return ResultType::Success;
}

bool PushEventProcessor::processRejected(const model::PushNotificationContext& context,
                                         const model::BatchResponse::Result& result)
{
    if (! result.rejected.empty())
    {
        for (const auto& rejected : result.rejected)
        {
            TVLOG(1) << "deleting rejected pushkey " << rejected;
            auto erpDatabase = mServiceContext->erpDatabaseFactory();
            erpDatabase->deletePushKey(context.hashedKvnr(), model::PushKey{rejected});
            AuditDataCollector auditDataCollector;
            auditDataCollector.setAgentName("E-Rezept-Fachdienst");
            auditDataCollector.setAgentType(model::AuditEvent::AgentType::machine);
            auditDataCollector.setAgentWho("9-E-Rezept-Fachdienst");
            auditDataCollector.setDeviceId(ExporterDeviceId);
            auditDataCollector.setInsurantKvnr(context.hashedKvnr());
            auditDataCollector.setAction(model::AuditEvent::Action::del);
            auditDataCollector.setEventId(model::AuditEventId::POST_PUSHERS_SET_UNREGISTER_IN_EXPORTER);
            auditDataCollector.setPushDeviceDisplayName(context.device().displayName());
            erpDatabase->storeAuditEventData(auditDataCollector);
            erpDatabase->commitTransaction();
        }
        return true;
    }
    return false;
}

void PushEventProcessor::deletePushNotification(const model::PushNotificationContext& notificationContext,
                                                const std::string& reason)
{
    mJsonLogFactory() << KeyValue("event", reason)
                      << KeyValue("channel_id", notificationContext.pushNotification().channelId())
                      << KeyValue("prescription_id", notificationContext.pushNotification().identifier())
                      << KeyValue("kvnr", notificationContext.hashedKvnr().getLoggingId())
                      << KeyValue("retryCount", std::to_string(notificationContext.retryCount()));
    mServiceContext->transaction(TransactionMode::autocommit, [&](auto& db) {
        db.deletePushNotification(notificationContext.hashedKvnr(), notificationContext.eventId());
    });
}
