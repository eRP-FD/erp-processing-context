/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_DATABASE_DATABASE_HXX
#define ERP_PROCESSING_CONTEXT_DATABASE_DATABASE_HXX

#include "erp/crypto/RandomSource.hxx"
#include "erp/database/DatabaseConnectionInfo.hxx"
#include "erp/database/DatabaseModel.hxx"
#include "erp/hsm/ErpTypes.hxx"

#include <date/date.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace model
{
class AuditData;
class Binary;
class Bundle;
struct ChargeInformation;
class ChargeItem;
class Communication;
class Consent;
class ErxReceipt;
class Kvnr;
class MedicationDispense;
class MedicationDispenseId;
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

enum class CmacKeyCategory : int8_t;

/**
 * Facade for an external database.
 * In production that is PostgreSQL.
 * In tests it can be a collection of non-persistent C++ containers.
 */
class Database
{
public:
    static constexpr const char* expectedSchemaVersion = "19";

    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct TaskAndKey
    {
        model::Task task;
        std::optional<SafeString> key;
    };

    using Factory = std::function<std::unique_ptr<Database>(HsmPool&, KeyDerivation&)>;

    virtual ~Database (void) = default;

    virtual void commitTransaction() = 0;
    virtual void closeConnection() = 0;

    virtual std::string retrieveSchemaVersion() = 0;

    virtual void healthCheck() = 0;
    virtual std::optional<DatabaseConnectionInfo> getConnectionInfo() const
    {
        return std::nullopt;
    }

    virtual model::PrescriptionId storeTask(const model::Task& task) = 0;
    virtual void updateTaskStatusAndSecret(const model::Task& task) = 0;
    virtual void updateTaskStatusAndSecret(const model::Task& task, const SafeString& key) = 0;
    virtual void activateTask(const model::Task& task, const model::Binary& healthCareProviderPrescription) = 0;
    virtual void activateTask(const model::Task& task, const SafeString& key, const model::Binary& healthCareProviderPrescription) = 0;
    virtual void updateTaskMedicationDispenseReceipt(const model::Task& task,
                                                     const std::vector<model::MedicationDispense>& medicationDispenses,
                                                     const model::ErxReceipt& receipt) = 0;
    virtual void updateTaskMedicationDispenseReceipt(const model::Task& task, const SafeString& key,
                                                     const std::vector<model::MedicationDispense>& medicationDispenses,
                                                     const model::ErxReceipt& receipt) = 0;
    virtual void updateTaskClearPersonalData(const model::Task& task) = 0;

    virtual std::string storeAuditEventData(model::AuditData& auditData) = 0;
    virtual std::vector<model::AuditData> retrieveAuditEventData(
        const model::Kvnr& kvnr,
        const std::optional<Uuid>& id,
        const std::optional<model::PrescriptionId>& prescriptionId,
        const std::optional<UrlArguments>& search) = 0;

    virtual std::optional<TaskAndKey> retrieveTaskForUpdate (const model::PrescriptionId& taskId) = 0;
    [[nodiscard]] virtual std::tuple<std::optional<TaskAndKey>, std::optional<model::Binary>>
    retrieveTaskForUpdateAndPrescription(const model::PrescriptionId& taskId) = 0;

    virtual std::tuple<std::optional<model::Task>, std::optional<model::Bundle>> retrieveTaskAndReceipt(const model::PrescriptionId& taskId) = 0;
    virtual std::tuple<std::optional<TaskAndKey>, std::optional<model::Binary>> retrieveTaskAndPrescription(const model::PrescriptionId& taskId) = 0;
    virtual std::tuple<std::optional<model::Task>, std::optional<model::Binary>>
    retrieveTaskWithSecretAndPrescription(const model::PrescriptionId& taskId) = 0;
    [[nodiscard]] virtual std::tuple<std::optional<model::Task>, std::optional<model::Binary>, std::optional<model::Bundle>>
    retrieveTaskAndPrescriptionAndReceipt(const model::PrescriptionId& taskId) = 0;

    virtual std::vector<model::Task> retrieveAllTasksForPatient(const model::Kvnr& kvnr,
                                                                const std::optional<UrlArguments>& search) = 0;
    virtual std::vector<model::Task> retrieveAll160TasksWithAccessCode(const model::Kvnr& kvnr, const std::optional<UrlArguments>& search) = 0;
    virtual uint64_t countAllTasksForPatient (const model::Kvnr& kvnr, const std::optional<UrlArguments>& search) = 0;

    // @return <medications, hasNextPage>
    [[nodiscard]] virtual std::tuple<std::vector<model::MedicationDispense>, bool>
    retrieveAllMedicationDispenses(const model::Kvnr& kvnr, const std::optional<UrlArguments>& search) = 0;
    [[nodiscard]] virtual std::optional<model::MedicationDispense>
    retrieveMedicationDispense(const model::Kvnr& kvnr, const model::MedicationDispenseId& id) = 0;

    virtual CmacKey acquireCmac(const date::sys_days& validDate, const CmacKeyCategory& cmacType, RandomSource& randomSource = RandomSource::defaultSource()) = 0;

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
        const model::Kvnr& insurantA,
        const model::Kvnr& insurantB,
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

    virtual void storeConsent(const model::Consent& consent) = 0;
    virtual std::optional<model::Consent> retrieveConsent(const model::Kvnr& kvnr) = 0;
    [[nodiscard]] virtual bool clearConsent(const model::Kvnr& kvnr) = 0;

    virtual void storeChargeInformation(const ::model::ChargeInformation& chargeInformation) = 0;
    virtual void updateChargeInformation(const ::model::ChargeInformation& chargeInformation, const BlobId& blobId, const db_model::Blob& salt) = 0;

    [[nodiscard]] virtual ::std::vector<::model::ChargeItem>
    retrieveAllChargeItemsForInsurant(const model::Kvnr& kvnr,
                                      const std::optional<UrlArguments>& search) const = 0;

    [[nodiscard]] virtual ::model::ChargeInformation
    retrieveChargeInformation(const model::PrescriptionId& id) const = 0;
    [[nodiscard]] virtual std::tuple<::model::ChargeInformation, BlobId, db_model::Blob>
    retrieveChargeInformationForUpdate(const model::PrescriptionId& id) const = 0;

    virtual void deleteChargeInformation(const model::PrescriptionId& id) = 0;
    virtual void clearAllChargeInformation(const model::Kvnr& kvnr) = 0;
    virtual void clearAllChargeItemCommunications(const model::Kvnr& kvnr) = 0;
    virtual void deleteCommunicationsForChargeItem(const model::PrescriptionId& id) = 0;
    virtual uint64_t countChargeInformationForInsurant (const model::Kvnr& kvnr, const std::optional<UrlArguments>& search) = 0;

    virtual DatabaseBackend& getBackend() = 0;
};

#endif
