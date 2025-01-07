/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/eventprocessing/EventProcessingBase.hxx"
#include "exporter/model/EpaErrorType.hxx"
#include "exporter/model/EpaOperationOutcome.hxx"
#include "exporter/model/TaskEvent.hxx"

#include <gsl/gsl-lite.hpp>
#include <utility>

namespace eventprocessing
{

EventProcessingBase::EventProcessingBase(gsl::not_null<IEpaMedicationClient*> client)
    : mMedicationClient(std::move(client))
{
}

void EventProcessingBase::logFailure(HttpStatus httpStatus, model::NumberAsStringParserDocument&& doc)
{
    if (httpStatus == HttpStatus::BadRequest)
    {
        model::EPAOperationOutcome operationOutcome(model::OperationOutcome::fromJson(std::move(doc)));
        for (const auto& issue : operationOutcome.getIssues())
        {
            TLOG(WARNING) << "EPAOperationOutcome code: " << magic_enum::enum_name(issue.detailsCode);
        }
    }
    else
    {
        model::EpaErrorType errorType(doc);
        TLOG(WARNING) << "ErrorType code: " << errorType.getErrorCode();
        if (! errorType.getErrorDetail().empty())
        {
            TLOG(WARNING) << "ErrorType detail: " << errorType.getErrorDetail();
        }
    }
}

JsonLog EventProcessingBase::logInfo(const model::TaskEvent& event)
{
    return log(JsonLog::makeInfoLogReceiver(), event);
}

JsonLog EventProcessingBase::logWarning(const model::TaskEvent& event)
{
    return log(JsonLog::makeWarningLogReceiver(), event);
}

JsonLog EventProcessingBase::logError(const model::TaskEvent& event)
{
    return log(JsonLog::makeErrorLogReceiver(), event);
}

JsonLog EventProcessingBase::log(JsonLog::LogReceiver&& logReceiver, const model::TaskEvent& event)
{
    LogId logId = LogId::INFO;
    JsonLog log(logId, std::move(logReceiver), false);
    log.keyValue("prescription_id", event.getPrescriptionId().toString())
        .keyValue("usecase", magic_enum::enum_name(event.getUseCase()));
    return log;
}


}
