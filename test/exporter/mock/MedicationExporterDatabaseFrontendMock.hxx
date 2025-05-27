/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#ifndef ERP_PROCESSING_CONTEXT_TEST_MEDICATIONEXPORTERDATABASEFRONTENDMOCK_HXX_INCLUDED
#define ERP_PROCESSING_CONTEXT_TEST_MEDICATIONEXPORTERDATABASEFRONTENDMOCK_HXX_INCLUDED

#ifdef Expect
#undef Expect
#include <gmock/gmock.h>
#include "shared/util/Expect.hxx"
#else
#include <gmock/gmock.h>
#endif

#include "exporter/database/MedicationExporterDatabaseFrontendInterface.hxx"
#include "exporter/model/EventKvnr.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "shared/database/DatabaseConnectionInfo.hxx"


class MedicationExporterDatabaseFrontendMock : public MedicationExporterDatabaseFrontendInterface
{
public:
    MOCK_METHOD(void, commitTransaction, (), (const, override));
    MOCK_METHOD(void, closeConnection, (), (override));
    MOCK_METHOD(std::string, retrieveSchemaVersion, (), (override));
    MOCK_METHOD(void, healthCheck, (), (override));
    MOCK_METHOD(std::optional<DatabaseConnectionInfo>, getConnectionInfo, (), (const, override));
    MOCK_METHOD(MedicationExporterDatabaseBackend&, getBackend, (), (override));
    MOCK_METHOD(std::optional<model::EventKvnr>, processNextKvnr, (), (const, override));
    MOCK_METHOD(taskevents_t, getAllEventsForKvnr, (const model::EventKvnr& eventKvnr), (const, override));
    MOCK_METHOD(bool, isDeadLetter,
                (const model::EventKvnr& kvnr, const model::PrescriptionId& prescriptionId,
                 model::PrescriptionType prescriptionType),
                (const, override));
    MOCK_METHOD(int, markDeadLetter,
                (const model::EventKvnr& kvnr, const model::PrescriptionId& prescriptionId,
                 model::PrescriptionType prescriptionType),
                (const, override));
    MOCK_METHOD(std::optional<model::BareTaskEvent>, markFirstEventDeadLetter, (const model::EventKvnr& kvnr),
                (const, override));
    MOCK_METHOD(void, deleteOneEventForKvnr, (const model::EventKvnr& kvnr, model::TaskEvent::id_t id),
                (const, override));
    MOCK_METHOD(void, deleteAllEventsForKvnr, (const model::EventKvnr& kvnr), (const, override));
    MOCK_METHOD(void, updateProcessingDelay,
                (std::int32_t newRetry, std::chrono::seconds delay, const model::EventKvnr& kvnr), (const, override));
    MOCK_METHOD(void, finalizeKvnr, (const model::EventKvnr& kvnr), (const, override));
};

class MedicationExporterDatabaseFrontendProxy : public MedicationExporterDatabaseFrontendInterface
{
public:

    MedicationExporterDatabaseFrontendProxy(gsl::not_null<MedicationExporterDatabaseFrontendInterface*> database);


    void commitTransaction() const override;
    void closeConnection() override;
    std::string retrieveSchemaVersion() override;
    void healthCheck() override;
    std::optional<DatabaseConnectionInfo> getConnectionInfo() const override;
    MedicationExporterDatabaseBackend& getBackend() override;
    std::optional<model::EventKvnr> processNextKvnr() const override;
    taskevents_t getAllEventsForKvnr(const model::EventKvnr& eventKvnr) const override;
    bool isDeadLetter(const model::EventKvnr& kvnr, const model::PrescriptionId& prescriptionId,
                      model::PrescriptionType prescriptionType) const override;
    int markDeadLetter(const model::EventKvnr& kvnr, const model::PrescriptionId& prescriptionId,
                       model::PrescriptionType prescriptionType) const override;
    std::optional<model::BareTaskEvent> markFirstEventDeadLetter(const model::EventKvnr& kvnr) const override;
    void deleteOneEventForKvnr(const model::EventKvnr& kvnr, model::TaskEvent::id_t id) const override;
    void deleteAllEventsForKvnr(const model::EventKvnr& kvnr) const override;
    void updateProcessingDelay(std::int32_t newRetry, std::chrono::seconds delay,
                               const model::EventKvnr& kvnr) const override;
    void finalizeKvnr(const model::EventKvnr& kvnr) const override;

private:
    gsl::not_null<MedicationExporterDatabaseFrontendInterface*> mDatabase;
};


#endif// ERP_PROCESSING_CONTEXT_TEST_MEDICATIONEXPORTERDATABASEFRONTENDMOCK_HXX_INCLUDED
