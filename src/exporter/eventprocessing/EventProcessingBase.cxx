/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/eventprocessing/EventProcessingBase.hxx"
#include "exporter/BdeMessage.hxx"
#include "exporter/model/EpaErrorType.hxx"
#include "exporter/model/EpaOperationOutcome.hxx"
#include "exporter/model/HashedKvnr.hxx"
#include "exporter/model/TaskEvent.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"

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
    JsonLog log(LogId::INFO, std::move(logReceiver), false);
    log << model::HashedKvnr(event.getHashedKvnr()) << KeyValue("prescription_id", event.getPrescriptionId().toString())
        << KeyValue("usecase", magic_enum::enum_name(event.getUseCase()))
        << KeyValue("x_request_id", event.getXRequestId());
    return log;
}

std::optional<std::string> EventProcessingBase::getFailureDiagnostics(const model::NumberAsStringParserDocument& doc)
{
    auto oo = model::OperationOutcome::fromJson(doc);
    if (! oo.issues().empty() && oo.issues()[0].diagnostics.has_value())
    {
        return oo.issues()[0].diagnostics;
    }
    return std::nullopt;
}

}
