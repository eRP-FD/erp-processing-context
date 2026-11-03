/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/eventprocessing/EventProcessingBase.hxx"
#include "shared/model/HashedKvnr.hxx"
#include "exporter/BdeMessage.hxx"
#include "exporter/model/EpaErrorType.hxx"
#include "exporter/model/EpaOperationOutcome.hxx"
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

std::string EventProcessingBase::getFailureDiagnostics(HttpStatus httpStatus,
                                                       const model::NumberAsStringParserDocument& doc)
{
    std::optional<std::string> ret;

    const auto oo = model::OperationOutcome::fromJson(doc);
    if (const auto issues = oo.issues(); ! issues.empty())
    {
        ret = issues.at(0).diagnostics;
        if (! ret)
        {
            ret = issues.at(0).detailsText;
        }
    }
    if (! ret)
    {
        const model::EpaErrorType errorType(doc);
        if (! errorType.getErrorDetail().empty())
        {
            ret = errorType.getErrorDetail();
        }
        if (! ret && ! errorType.getErrorCode().empty())
        {
            ret = errorType.getErrorCode();
        }
    }
    return ret.value_or(toString(httpStatus));
}

}
