/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "BdeMessage.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/TLog.hxx"
#include "shared/util/Uuid.hxx"

#include <boost/asio/awaitable.hpp>
#include <functional>
#include <memory>
#include <optional>

class RunLoopScheduler;

namespace boost::asio
{
class io_context;
}

namespace medication::exporter::exceptions
{
class OperationException;
};

namespace model
{
class Bundle;
class ErpTPrescriptionCarbonCopy;
class EventKvnr;
class TRezeptEvent;
}

class MedicationExporterServiceContext;
class JsonLog;

class TRezeptEventProcessor
{
public:
    enum class ResultType : uint8_t
    {
        Idle,
        Success,
        Retry,
        Failure,
        FailureOther
    };

    explicit TRezeptEventProcessor(const std::shared_ptr<MedicationExporterServiceContext>& serviceContext);

    ResultType process();

    static std::chrono::seconds getExponentialBackoffTime();

    static boost::asio::awaitable<void> runloopWorker(RunLoopScheduler& scheduler,
                                                      std::weak_ptr<MedicationExporterServiceContext> serviceContext);

    const std::string& xContext() const;

private:
    ResultType doProcess();

    std::optional<model::Bundle> runFhirVzdSearch(const model::TRezeptEvent& event);
    std::optional<model::ErpTPrescriptionCarbonCopy> createCarbonCopy(const model::TRezeptEvent& eventData,
                                                                      const model::Bundle& vzdSearchBundle);
    ResultType sendCarbonCopy(const model::TRezeptEvent& event, const model::ErpTPrescriptionCarbonCopy& cc);

    void deleteTRezeptEvent(const model::TRezeptEvent& eventData);
    void scheduleRetryTRezeptEvent(const model::TRezeptEvent& eventData);
    void markDeadLetterTRezeptEvent(const model::TRezeptEvent& eventData);

    TRezeptEventProcessor::ResultType
    handleOperationalBfarmError(const model::TRezeptEvent& event,
                                const medication::exporter::exceptions::OperationException& exc);

    template<typename FuncT>
    decltype(auto) autocommit(FuncT&& function);

    std::function<JsonLog()> mJsonLog;

    const std::shared_ptr<MedicationExporterServiceContext>& mServiceContext;

    int mMaxRetries;
    static std::atomic<int> mRetryCount;
    std::string mXContext{Uuid{}.toString()};
    ScopedLogContext mLogContext;
};
