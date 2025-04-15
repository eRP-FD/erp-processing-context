/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESSING_CANCELPRESCRIPTION_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESSING_CANCELPRESCRIPTION_HXX

#include "exporter/eventprocessing/EventProcessingBase.hxx"

namespace model
{
class CancelPrescriptionTaskEvent;
}
namespace eventprocessing
{

class CancelPrescription : public EventProcessingBase
{
public:
    static Outcome process(gsl::not_null<IEpaMedicationClient*> client, const model::CancelPrescriptionTaskEvent& erpEvent);

private:
    explicit CancelPrescription(gsl::not_null<IEpaMedicationClient*> medicationClient);
    Outcome doProcess(const model::CancelPrescriptionTaskEvent& erpEvent);
};

}


#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESSING_CANCELPRESCRIPTION_HXX
