/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_DATABASE_MEDICATIONEXPORTER_BACKEND_HXX
#define ERP_PROCESSING_CONTEXT_DATABASE_MEDICATIONEXPORTER_BACKEND_HXX

#include "exporter/database/ExporterDatabaseModel.hxx"
#include "exporter/model/TaskEvent.hxx"
#include "shared/database/DatabaseBackend.hxx"
#include "shared/database/DatabaseModel.hxx"

#include <optional>
#include <vector>

namespace model
{
class EventKvnr;
}

class MedicationExporterDatabaseBackend : virtual public DatabaseBackend
{
public:
    MedicationExporterDatabaseBackend() = default;
    ~MedicationExporterDatabaseBackend() override = default;

    virtual std::optional<model::EventKvnr> processNextKvnr() = 0;
    virtual std::vector<db_model::TaskEvent> getAllEventsForKvnr(const model::EventKvnr& kvnr) = 0;
    virtual bool isDeadLetter(const model::EventKvnr& kvnr, const model::PrescriptionId& prescriptionId,
                              model::PrescriptionType prescriptionType) = 0;
    virtual int markDeadLetter(const model::EventKvnr& kvnr, const model::PrescriptionId& prescriptionId,
                               model::PrescriptionType prescriptionType) = 0;
    virtual void deleteOneEventForKvnr(const model::EventKvnr& kvnr, model::TaskEvent::id_t id) = 0;
    virtual void deleteAllEventsForKvnr(const model::EventKvnr& kvnr) = 0;
    virtual void postponeProcessing(std::chrono::seconds delay, const model::EventKvnr& kvnr) = 0;
    virtual void finalizeKvnr(const model::EventKvnr& kvnr) const = 0;
};

#endif// ERP_PROCESSING_CONTEXT_DATABASE_MEDICATIONEXPORTER_BACKEND_HXX
