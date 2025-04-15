/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESING_PROVIDEPRESCRIPTION_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESING_PROVIDEPRESCRIPTION_HXX

#include "exporter/eventprocessing/EventProcessingBase.hxx"

namespace eventprocessing
{

class ProvidePrescription : public EventProcessingBase
{
public:
    static Outcome process(gsl::not_null<IEpaMedicationClient*> client, const model::ProvidePrescriptionTaskEvent& erpEvent);

private:
    explicit ProvidePrescription(gsl::not_null<IEpaMedicationClient*> medicationClient);
    Outcome doProcess(const model::ProvidePrescriptionTaskEvent& taskEvent) const;
};

}// eventprocessing

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESING_PROVIDEPRESCRIPTION_HXX
