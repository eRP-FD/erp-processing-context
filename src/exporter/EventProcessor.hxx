/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_EXPORTER_EVENTPROCESSOR_HXX
#define ERP_PROCESSING_CONTEXT_EXPORTER_EVENTPROCESSOR_HXX

#include "EpaAccountLookup.hxx"
#include "database/CommitGuard.hxx"
#include "database/MedicationExporterDatabaseFrontendInterface.hxx"
#include "model/EventKvnr.hxx"
#include "shared/audit/AuditDataCollector.hxx"
#include "shared/util/JsonLog.hxx"
#include "shared/util/Configuration.hxx"

#include <memory>

class IEpaMedicationClient;
class MedicationExporterDatabaseFrontendInterface;
class MedicationExporterServiceContext;
class SafeString;

class EventProcessorInterface
{
public:
    virtual ~EventProcessorInterface() = default;
    virtual bool process() = 0;
};

class EventProcessor : public EventProcessorInterface
{
public:
    explicit EventProcessor(const std::shared_ptr<MedicationExporterServiceContext>& serviceContext);

    bool process() override;
    virtual void processOne(const model::EventKvnr& kvnr);

    virtual void processEpaAllowed(const model::EventKvnr& kvnr, EpaAccount& epaAccount,
                                   const MedicationExporterDatabaseFrontendInterface::taskevents_t& events);
    virtual void processEpaDeniedOrNotFound(const model::EventKvnr& kvnr);
    virtual void processEpaConflict(const model::EventKvnr& kvnr);
    virtual void processEpaUnknown(const model::EventKvnr& kvnr);

    virtual EpaAccount ePaAccountLookup(const model::TaskEvent& taskEvent);

private:
    std::function<JsonLog()> jsonLog;
    const std::shared_ptr<MedicationExporterServiceContext>& mServiceContext;
    std::chrono::seconds mRetryDelaySeconds;
    std::chrono::seconds mConflictDelaySeconds;
    void writeAuditEvent(const AuditDataCollector& auditDataCollector);
    MedicationExporterCommitGuard createMedicationExporterCommitGuard();
};

#endif//#ifndef ERP_PROCESSING_CONTEXT_EXPORTER_EVENTPROCESSOR_HXX