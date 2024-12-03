/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_DATABASE_MEDICATIONEXPORTERFRONTENDINTERFACE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_DATABASE_MEDICATIONEXPORTERFRONTENDINTERFACE_HXX

#include "exporter/model/TaskEvent.hxx"

#include <functional>
#include <memory>
#include <optional>


namespace model
{
class AuditData;
class EventKvnr;
}

class HsmPool;
class KeyDerivation;
class MedicationExporterDatabaseBackend;

struct DatabaseConnectionInfo;


class MedicationExporterDatabaseFrontendInterface
{
public:
    static constexpr const char* expectedSchemaVersion = "1";

    using taskevents_t = std::vector<std::unique_ptr<model::TaskEvent>>;

    using Factory =
        std::function<std::unique_ptr<MedicationExporterDatabaseFrontendInterface>(HsmPool&, KeyDerivation&)>;

    virtual ~MedicationExporterDatabaseFrontendInterface() = default;

    virtual void commitTransaction() const = 0;

    virtual void closeConnection() = 0;

    virtual std::string retrieveSchemaVersion() = 0;

    virtual void healthCheck() = 0;

    virtual std::optional<DatabaseConnectionInfo> getConnectionInfo() const = 0;

    virtual MedicationExporterDatabaseBackend& getBackend() = 0;

    virtual std::optional<model::EventKvnr> processNextKvnr() const = 0;
    virtual taskevents_t getAllEventsForKvnr(const model::EventKvnr& eventKvnr) const = 0;
    virtual bool isDeadLetter(const model::EventKvnr& kvnr, const model::PrescriptionId& prescriptionId,
                              model::PrescriptionType prescriptionType) const = 0;
    virtual int markDeadLetter(const model::EventKvnr& kvnr, const model::PrescriptionId& prescriptionId,
                               model::PrescriptionType prescriptionType) const = 0;
    virtual void deleteOneEventForKvnr(const model::EventKvnr& kvnr, model::TaskEvent::id_t id) const = 0;
    virtual void deleteAllEventsForKvnr(const model::EventKvnr& kvnr) const = 0;
    virtual void postponeProcessing(std::chrono::seconds delay, const model::EventKvnr& kvnr) const = 0;
    virtual void finalizeKvnr(const model::EventKvnr& kvnr) const = 0;
};

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_DATABASE_MEDICATIONEXPORTERFRONTENDINTERFACE_HXX
