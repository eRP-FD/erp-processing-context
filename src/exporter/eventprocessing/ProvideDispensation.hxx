/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESSING_PROVIDEDISPENSATION_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESSING_PROVIDEDISPENSATION_HXX

#include "exporter/eventprocessing/EventProcessingBase.hxx"

namespace eventprocessing
{

class ProvideDispensation : public EventProcessingBase
{
public:
    static Outcome process(gsl::not_null<IEpaMedicationClient*> client, const model::ProvideDispensationTaskEvent& erpEvent);

private:
    explicit ProvideDispensation(gsl::not_null<IEpaMedicationClient*> medicationClient);
    Outcome doProcess(const model::ProvideDispensationTaskEvent& taskEvent);
};

}// eventprocessing

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESSING_PROVIDEDISPENSATION_HXX
