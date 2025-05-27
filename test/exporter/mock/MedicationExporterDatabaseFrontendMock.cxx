/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/exporter/mock/MedicationExporterDatabaseFrontendMock.hxx"

MedicationExporterDatabaseFrontendProxy::MedicationExporterDatabaseFrontendProxy(
    gsl::not_null<MedicationExporterDatabaseFrontendInterface*> database)
    : mDatabase{database}
{
}

void MedicationExporterDatabaseFrontendProxy::commitTransaction() const
{
    mDatabase->commitTransaction();
}

void MedicationExporterDatabaseFrontendProxy::closeConnection()
{
    mDatabase->closeConnection();
}
std::string MedicationExporterDatabaseFrontendProxy::retrieveSchemaVersion()
{
    return mDatabase->retrieveSchemaVersion();
}

void MedicationExporterDatabaseFrontendProxy::healthCheck()
{
    mDatabase->healthCheck();
}

std::optional<DatabaseConnectionInfo> MedicationExporterDatabaseFrontendProxy::getConnectionInfo() const
{
    return mDatabase->getConnectionInfo();
}

MedicationExporterDatabaseBackend& MedicationExporterDatabaseFrontendProxy::getBackend()
{
    return mDatabase->getBackend();
}

std::optional<model::EventKvnr> MedicationExporterDatabaseFrontendProxy::processNextKvnr() const
{
    return mDatabase->processNextKvnr();
}

MedicationExporterDatabaseFrontendInterface::taskevents_t
MedicationExporterDatabaseFrontendProxy::getAllEventsForKvnr(const model::EventKvnr& eventKvnr) const
{
    return mDatabase->getAllEventsForKvnr(eventKvnr);
}

bool MedicationExporterDatabaseFrontendProxy::isDeadLetter(const model::EventKvnr& kvnr,
                                                           const model::PrescriptionId& prescriptionId,
                                                           model::PrescriptionType prescriptionType) const
{
    return mDatabase->isDeadLetter(kvnr, prescriptionId, prescriptionType);
}
int MedicationExporterDatabaseFrontendProxy::markDeadLetter(const model::EventKvnr& kvnr,
                                                            const model::PrescriptionId& prescriptionId,
                                                            model::PrescriptionType prescriptionType) const
{
    return mDatabase->markDeadLetter(kvnr, prescriptionId, prescriptionType);
}

std::optional<model::BareTaskEvent>
MedicationExporterDatabaseFrontendProxy::markFirstEventDeadLetter(const model::EventKvnr& kvnr) const
{
    return mDatabase->markFirstEventDeadLetter(kvnr);
}

void MedicationExporterDatabaseFrontendProxy::deleteOneEventForKvnr(const model::EventKvnr& kvnr,
                                                                    model::TaskEvent::id_t id) const
{
    mDatabase->deleteOneEventForKvnr(kvnr, id);
}

void MedicationExporterDatabaseFrontendProxy::deleteAllEventsForKvnr(const model::EventKvnr& kvnr) const
{
    mDatabase->deleteAllEventsForKvnr(kvnr);
}

void MedicationExporterDatabaseFrontendProxy::updateProcessingDelay(std::int32_t newRetry, std::chrono::seconds delay,
                                                                    const model::EventKvnr& kvnr) const
{
    return mDatabase->updateProcessingDelay(newRetry, delay, kvnr);
}

void MedicationExporterDatabaseFrontendProxy::finalizeKvnr(const model::EventKvnr& kvnr) const
{
    mDatabase->finalizeKvnr(kvnr);
}
