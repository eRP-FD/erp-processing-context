/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MEDICATIONEXPORTERDATABASEFRONTEND_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MEDICATIONEXPORTERDATABASEFRONTEND_HXX

#include "exporter/TelematikLookup.hxx"
#include "exporter/database/ExporterDatabaseModel.hxx"
#include "exporter/database/MedicationExporterDatabaseFrontendInterface.hxx"
#include "shared/database/DatabaseCodec.hxx"
#include "shared/hsm/KeyDerivation.hxx"
#include "shared/util/SafeString.hxx"

#include <optional>


class MedicationExporterDatabaseFrontend : public MedicationExporterDatabaseFrontendInterface
{
public:
    MedicationExporterDatabaseFrontend(std::unique_ptr<MedicationExporterDatabaseBackend>&& backend,
                                       KeyDerivation& keyDerivation, const TelematikLookup& lookup);

    ~MedicationExporterDatabaseFrontend() override = default;

    void commitTransaction() const override;

    void closeConnection() override;

    std::string retrieveSchemaVersion() override;

    void healthCheck() override;

    std::optional<DatabaseConnectionInfo> getConnectionInfo() const override;

    [[nodiscard]] MedicationExporterDatabaseBackend& getBackend() override;

    std::optional<model::EventKvnr> processNextKvnr() const override;

    taskevents_t getAllEventsForKvnr(const model::EventKvnr& eventKvnr) const override;

    bool isDeadLetter(const model::EventKvnr& kvnr, const model::PrescriptionId& prescriptionId,
                      model::PrescriptionType prescriptionType) const override;

    int markDeadLetter(const model::EventKvnr& kvnr, const model::PrescriptionId& prescriptionId,
                       model::PrescriptionType prescriptionType) const override;

    void deleteOneEventForKvnr(const model::EventKvnr& kvnr, model::TaskEvent::id_t id) const override;

    void deleteAllEventsForKvnr(const model::EventKvnr& kvnr) const override;

    void updateProcessingDelay(std::int32_t newRetry, std::chrono::seconds delay, const model::EventKvnr& kvnr) const override;

    void finalizeKvnr(const model::EventKvnr& kvnr) const override;

private:
    static std::shared_ptr<Compression> compressionInstance();
    [[nodiscard]] SafeString taskKey(const db_model::TaskEvent& dbTaskEvent) const;
    [[nodiscard]] SafeString medicationDispenseKey(const db_model::TaskEvent& dbTaskEvent) const;

    std::unique_ptr<MedicationExporterDatabaseBackend> mBackend;
    KeyDerivation& mDerivation;
    DataBaseCodec mCodec;
    const TelematikLookup& mTelematikLookup;
};

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MEDICATIONEXPORTERDATABASEFRONTEND_HXX
