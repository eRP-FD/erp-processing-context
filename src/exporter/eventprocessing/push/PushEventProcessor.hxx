/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "exporter/model/push/BatchResponse.hxx"
#include "shared/database/DatabaseModel.hxx"
#include "shared/util/JsonLog.hxx"
#include "shared/util/TLog.hxx"
#include "shared/util/Uuid.hxx"

#include <boost/asio/awaitable.hpp>
#include <gsl/gsl-lite.hpp>
#include <memory>


class PushGatewayClient;
class ClientResponse;
namespace model
{
class Kvnr;
class PushNotificationContext;
}
class MedicationExporterServiceContext;
class RunLoopScheduler;

class PushEventProcessor
{
public:
    enum class ResultType : uint8_t
    {
        Idle,
        Success,
        FailureRetry,
        FailureRejected,
    };

    static boost::asio::awaitable<void> runloopWorker(RunLoopScheduler& scheduler,
                                                      std::weak_ptr<MedicationExporterServiceContext> serviceContext);

    explicit PushEventProcessor(const std::shared_ptr<MedicationExporterServiceContext>& serviceContext,
                                gsl::not_null<PushGatewayClient*> client);

    ResultType process();
    ResultType processEvents(const std::vector<model::PushNotificationContext>& events);
    ResultType processEvent(const model::PushNotificationContext& event);
    std::vector<model::PushNotificationContext> fetchNextEvent();
    ResultType processResponse(const model::PushNotificationContext& event, const ClientResponse& response,
                               std::chrono::milliseconds httpResponseTime, std::chrono::milliseconds duration);
    [[nodiscard]] bool processRejected(const model::PushNotificationContext& context,
                                       const model::BatchResponse::Result& result);
    void deletePushNotification(const model::PushNotificationContext& notificationContext, const std::string& reason);

private:
    std::shared_ptr<MedicationExporterServiceContext> mServiceContext;
    gsl::not_null<PushGatewayClient*> mClient;
    std::function<JsonLog()> mJsonLogFactory;
    std::string mXContext{Uuid{}.toString()};
    ScopedLogContext mLogContext;
};
