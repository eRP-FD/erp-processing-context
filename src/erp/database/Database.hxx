/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_DATABASE_DATABASE_HXX
#define ERP_PROCESSING_CONTEXT_DATABASE_DATABASE_HXX

#include "erp/crypto/RandomSource.hxx"

#include <date/date.h>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace model
{
class AuditData;
class Binary;
class Bundle;
class Communication;
class ErxReceipt;
class MedicationDispense;
class PrescriptionId;
class Task;
class Timestamp;
}

class CmacKey;
class HsmPool;
class DatabaseBackend;
class KeyDerivation;
class UrlArguments;
class Uuid;

/**
 * Facade for an external database.
 * In production that is PostgreSQL.
 * In tests it can be a collection of non-persistent C++ containers.
 */
class Database
{
public:
    using Factory = std::function<std::unique_ptr<Database>(HsmPool&, KeyDerivation&)>;

    virtual ~Database (void) = default;

    virtual void commitTransaction() = 0;
    virtual void closeConnection() = 0;

    virtual void healthCheck() = 0;

    virtual model::PrescriptionId storeTask(const model::Task& task) = 0;
    virtual void updateTaskStatusAndSecret(const model::Task& task) = 0;
    virtual void activateTask(const model::Task& task, const model::Binary& healthCareProviderPrescription) = 0;
    virtual void updateTaskMedicationDispenseReceipt(const model::Task& task, const model::MedicationDispense& medicationDispense, const model::ErxReceipt& receipt) = 0;
    virtual void updateTaskClearPersonalData(const model::Task& task) = 0;

    virtual std::string storeAuditEventData(model::AuditData& auditData) = 0;
    virtual std::vector<model::AuditData> retrieveAuditEventData(
        const std::string& kvnr,
        const std::optional<Uuid>& id,
        const std::optional<model::PrescriptionId>& prescriptionId,
        const std::optional<UrlArguments>& search) = 0;
    virtual uint64_t countAuditEventData(
        const std::string& kvnr,
        const std::optional<UrlArguments>& search) = 0;

    virtual std::optional<model::Task> retrieveTask (const model::PrescriptionId& taskId) = 0;
    virtual std::optional<model::Task> retrieveTaskForUpdate (const model::PrescriptionId& taskId) = 0;

    virtual std::tuple<std::optional<model::Task>, std::optional<model::Bundle>> retrieveTaskAndReceipt(const model::PrescriptionId& taskId) = 0;
    virtual std::tuple<std::optional<model::Task>, std::optional<model::Binary>> retrieveTaskAndPrescription(const model::PrescriptionId& taskId) = 0;
    virtual std::vector<model::Task> retrieveAllTasksForPatient (const std::string& kvnr, const std::optional<UrlArguments>& search) = 0;
    virtual uint64_t countAllTasksForPatient (const std::string& kvnr, const std::optional<UrlArguments>& search) = 0;

    virtual std::vector<model::MedicationDispense> retrieveAllMedicationDispenses(
        const std::string& kvnr,
        const std::optional<model::PrescriptionId>& prescriptionId,
        const std::optional<UrlArguments>& search) = 0;
    virtual uint64_t countAllMedicationDispenses(
        const std::string& kvnr,
        const std::optional<UrlArguments>& search) = 0;

    virtual CmacKey acquireCmac(const date::sys_days& validDate, RandomSource& randomSource = RandomSource::defaultSource()) = 0;

    /**
     * Insert the `communication` object into the database.
     * The automatically created id is both set at the given object and returned.
     */
    virtual std::optional<Uuid> insertCommunication(model::Communication& communication) = 0;
    /**
     * Count communication objects between two insurants or representatives, typically the `sender` and the `recipient`
     * of a Communication object, and task with the given `prescriptionId`.
     * Note that sender and recipient can be interchanged, i.e. direction of the communication is ignored.
     */
    virtual uint64_t countRepresentativeCommunications(
        const std::string& insurantA,
        const std::string& insurantB,
        const model::PrescriptionId& prescriptionId) = 0;
    /**
      * Checks whether a communication object with the given `communicationId` exists in the database.
      */
    virtual bool existCommunication(const Uuid& communicationId) = 0;
    /**
     * Retrieve all communication objects to or from the given `user` (insurant or pharmacy) with
     * an optional filter on `communicationId` (for getById) and an optional `search` object (for getAll).
     */
    virtual std::vector<model::Communication> retrieveCommunications (
        const std::string& user,
        const std::optional<Uuid>& communicationId,
        const std::optional<UrlArguments>& search) = 0;
    /**
     * Count communication objects either sent or received by the given user.
     * Note that sender and recipient can be interchanged, i.e. direction of the communication is ignored.
     */
    virtual uint64_t countCommunications(
        const std::string& user,
        const std::optional<UrlArguments>& search) = 0;

    /**
     * For tests, retrieve only the database ids of matching communication objects so that these
     * can be counted or deleted.
     */
    virtual std::vector<Uuid> retrieveCommunicationIds (const std::string& recipient) = 0;
    /**
     * Tries to remove the communication object defined by its id and sender from the database.
     * If no data row for the specified id and sender has been found the method returns no value.
     * If the data row has been deleted a tuple consisting of the id as the first element and
     * the optional value of the column "received"a as the second element is returned.
     * If "received" does not have a value this means that the communication has not been
     * received by the recipient.
     */
    virtual std::tuple<std::optional<Uuid>, std::optional<model::Timestamp>> deleteCommunication (const Uuid& communicationId, const std::string& sender) = 0;
    virtual void deleteCommunicationsForTask (const model::PrescriptionId& taskId) = 0;
    virtual void markCommunicationsAsRetrieved (
        const std::vector<Uuid>& communicationIds,
        const model::Timestamp& retrieved,
        const std::string& recipient) = 0;

    virtual DatabaseBackend& getBackend() = 0;
};

#endif
