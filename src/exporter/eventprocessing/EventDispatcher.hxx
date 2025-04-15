/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESING_EVENTDISPATCHER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESING_EVENTDISPATCHER_HXX

#include "exporter/eventprocessing/Outcome.hxx"

namespace model
{
class TaskEvent;
}

class IEpaMedicationClient;
class AuditDataCollector;

namespace eventprocessing
{

class EventDispatcher
{
public:
    explicit EventDispatcher(std::unique_ptr<IEpaMedicationClient>&& medicationClient);
    Outcome dispatch(const model::TaskEvent& erpEvent, AuditDataCollector& auditDataCollector);

private:
    std::unique_ptr<IEpaMedicationClient> mMedicationClient;
};

}// eventprocessing

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESING_EVENTDISPATCHER_HXX
