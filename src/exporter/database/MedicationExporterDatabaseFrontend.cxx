/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "MedicationExporterDatabaseFrontend.hxx"
#include "erp/crypto/SignedPrescription.hxx"
#include "erp/model/Binary.hxx"
#include "exporter/database/MedicationExporterDatabaseBackend.hxx"
#include "exporter/database/TaskEventConverter.hxx"
#include "exporter/model/EventKvnr.hxx"
#include "exporter/model/TaskEvent.hxx"
#include "shared/compression/ZStd.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/util/Configuration.hxx"

MedicationExporterDatabaseFrontend::MedicationExporterDatabaseFrontend(
    std::unique_ptr<MedicationExporterDatabaseBackend>&& backend, HsmPool& hsmPool, KeyDerivation& keyDerivation,
    const TelematikLookup& lookup)
    : mBackend(std::move(backend))
    , mHsmPool(hsmPool)
    , mDerivation(keyDerivation)
    , mCodec(compressionInstance())
    , mTelematikLookup(lookup)
{
}

void MedicationExporterDatabaseFrontend::commitTransaction() const
{
    mBackend->commitTransaction();
}

void MedicationExporterDatabaseFrontend::closeConnection()
{
    mBackend->closeConnection();
}

std::string MedicationExporterDatabaseFrontend::retrieveSchemaVersion()
{
    return mBackend->retrieveSchemaVersion();
}

void MedicationExporterDatabaseFrontend::healthCheck()
{
    mBackend->healthCheck();
}

std::optional<DatabaseConnectionInfo> MedicationExporterDatabaseFrontend::getConnectionInfo() const
{
    return mBackend->getConnectionInfo();
}

[[nodiscard]] MedicationExporterDatabaseBackend& MedicationExporterDatabaseFrontend::getBackend()
{
    return *mBackend;
}

std::shared_ptr<Compression> MedicationExporterDatabaseFrontend::compressionInstance()
{
    static auto theCompressionInstance =
        std::make_shared<ZStd>(Configuration::instance().getStringValue(ConfigurationKey::ZSTD_DICTIONARY_DIR));
    return theCompressionInstance;
}

std::optional<model::EventKvnr> MedicationExporterDatabaseFrontend::processNextKvnr() const
{
    std::optional<model::EventKvnr> results = mBackend->processNextKvnr();
    return results;
}

MedicationExporterDatabaseFrontendInterface::taskevents_t
MedicationExporterDatabaseFrontend::getAllEventsForKvnr(const model::EventKvnr& eventKvnr) const
{
    auto dbTaskEventList = mBackend->getAllEventsForKvnr(eventKvnr);
    std::vector<std::unique_ptr<model::TaskEvent>> allTaskEvents;
    for (const auto& dbTaskEvent : dbTaskEventList)
    {
        auto keyForTask = taskKey(dbTaskEvent);
        SafeString keyForMedicationDispense{
            dbTaskEvent.medicationDispenseBundle.has_value() ? medicationDispenseKey(dbTaskEvent) : SafeString{}};
        allTaskEvents.emplace_back(
            TaskEventConverter(mCodec, mTelematikLookup).convert(dbTaskEvent, keyForTask, keyForMedicationDispense));
    }
    return allTaskEvents;
}

bool MedicationExporterDatabaseFrontend::isDeadLetter(const model::EventKvnr& kvnr,
                                                      const model::PrescriptionId& prescriptionId,
                                                      model::PrescriptionType prescriptionType) const
{
    return mBackend->isDeadLetter(kvnr, prescriptionId, prescriptionType);
}

int MedicationExporterDatabaseFrontend::markDeadLetter(const model::EventKvnr& kvnr,
                                                       const model::PrescriptionId& prescriptionId,
                                                       model::PrescriptionType prescriptionType) const
{
    return mBackend->markDeadLetter(kvnr, prescriptionId, prescriptionType);
}

void MedicationExporterDatabaseFrontend::deleteOneEventForKvnr(const model::EventKvnr& kvnr,
                                                               model::TaskEvent::id_t id) const
{
    mBackend->deleteOneEventForKvnr(kvnr, id);
}

void MedicationExporterDatabaseFrontend::deleteAllEventsForKvnr(const model::EventKvnr& kvnr) const
{
    mBackend->deleteAllEventsForKvnr(kvnr);
}

void MedicationExporterDatabaseFrontend::updateProcessingDelay(std::int32_t newRetry, std::chrono::seconds delay, const model::EventKvnr& kvnr) const
{
    mBackend->updateProcessingDelay(newRetry, delay, kvnr);
}

void MedicationExporterDatabaseFrontend::finalizeKvnr(const model::EventKvnr& kvnr) const
{
    mBackend->finalizeKvnr(kvnr);
}

SafeString MedicationExporterDatabaseFrontend::taskKey(const db_model::TaskEvent& dbTaskEvent) const
{
    Expect(not dbTaskEvent.salt.empty(), "missing salt in task event.");
    return mDerivation.taskKey(dbTaskEvent.prescriptionId, dbTaskEvent.authoredOn, dbTaskEvent.blobId,
                               dbTaskEvent.salt);
}

SafeString MedicationExporterDatabaseFrontend::medicationDispenseKey(const db_model::TaskEvent& dbTaskEvent) const
{
    Expect(dbTaskEvent.medicationDispenseSalt, "missing medication dispense salt in task event.");
    Expect(dbTaskEvent.medicationDispenseBundleBlobId, "missing medication dispense blob id in task event.");
    return mDerivation.medicationDispenseKey(dbTaskEvent.hashedKvnr, *dbTaskEvent.medicationDispenseBundleBlobId,
                                             *dbTaskEvent.medicationDispenseSalt);
}
