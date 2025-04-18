/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESING_EVENTPROCESSINGBASE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESING_EVENTPROCESSINGBASE_HXX

#include "exporter/eventprocessing/Outcome.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/util/JsonLog.hxx"

#include <cstdint>
#include <gsl/gsl-lite.hpp>

class IEpaMedicationClient;

namespace model
{
class TaskEvent;
class ProvidePrescriptionTaskEvent;
class ProvideDispensationTaskEvent;
class NumberAsStringParserDocument;
}

namespace eventprocessing
{

class EventProcessingBase
{
protected:
    explicit EventProcessingBase(gsl::not_null<IEpaMedicationClient*> client);
    gsl::not_null<IEpaMedicationClient*> mMedicationClient;
    static void logFailure(HttpStatus httpStatus, model::NumberAsStringParserDocument&& doc);

    static JsonLog logInfo(const model::TaskEvent& event);
    static JsonLog logWarning(const model::TaskEvent& event);
    static JsonLog logError(const model::TaskEvent& event);
    static JsonLog log(JsonLog::LogReceiver&& logReceiver, const model::TaskEvent& event);

};

}// eventprocessing

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESING_EVENTPROCESSINGBASE_HXX
