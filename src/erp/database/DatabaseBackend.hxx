/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_DATABASEBACKEND_HXX
#define ERP_PROCESSING_CONTEXT_DATABASEBACKEND_HXX

#include "erp/database/DatabaseConnectionInfo.hxx"
#include "erp/hsm/ErpTypes.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/Task.hxx"

#include <date/date.h>
#include <optional>
#include <string>
#include <vector>

class CmacKey;
class KeyDerivation;
struct OptionalDeriveKeyData;
class RandomSource;
class UrlArguments;
class Uuid;


namespace db_model
{
enum class MasterKeyType: int8_t;

class Blob;
struct ChargeItem;
class Communication;
class EncryptedBlob;
class HashedId;
class HashedKvnr;
class HashedTelematikId;
class MedicationDispense;
class Task;
class AuditData;
}

namespace model
{
class Binary;
class Bundle;
class Communication;
class ErxReceipt;
class PrescriptionId;
}

enum class CmacKeyCategory: int8_t;

class DatabaseBackend
{
public:
    virtual ~DatabaseBackend(void) = default;

    virtual void commitTransaction() = 0;
    virtual void closeConnection() = 0;
    virtual bool isCommitted() const = 0;

    virtual std::string retrieveSchemaVersion() = 0;

    virtual void healthCheck() = 0;
    virtual std::optional<DatabaseConnectionInfo> getConnectionInfo() const
    {
        return std::nullopt;
    }

    /// @returns (pescription_id, authored_on)
    /// NOTE: The return value authored_on should be used for key derivation.
    ///       It has been passed through the database and has undergone the same rounding as will be done
    ///       when decrypting. Using the input parameter @p created might suffer from later rounding errors
    ///       see ERP-5602
    virtual std::tuple<model::PrescriptionId, model::Timestamp> createTask(model::PrescriptionType prescriptionType,
                                                                           model::Task::Status taskStatus,
                                                                           const model::Timestamp& lastUpdated,
                                                                           const model::Timestamp& created) = 0;

    virtual void updateTask(const model::PrescriptionId& taskId, const db_model::EncryptedBlob& accessCode,
                            BlobId blobId, const db_model::Blob& salt) = 0;
    /// @returns BlobId, salt, authoredOn
    virtual std::tuple<BlobId, db_model::Blob, model::Timestamp>
    getTaskKeyData(const model::PrescriptionId& taskId) = 0;

    virtual void updateTaskStatusAndSecret(const model::PrescriptionId& taskId, model::Task::Status status,
                                           const model::Timestamp& lastModifiedDate,
                                           const std::optional<db_model::EncryptedBlob>& secret) = 0;
    virtual void activateTask(const model::PrescriptionId& taskId, const db_model::EncryptedBlob& encryptedKvnr,
                              const db_model::HashedKvnr& hashedKvnr, model::Task::Status taskStatus,
                              const model::Timestamp& lastModified, const model::Timestamp& expiryDate,
                              const model::Timestamp& acceptDate,
                              const db_model::EncryptedBlob& healthCareProviderPrescription) = 0;
    virtual void updateTaskMedicationDispenseReceipt(const model::PrescriptionId& taskId,
                                                     const model::Task::Status& taskStatus,
                                                     const model::Timestamp& lastModified,
                                                     const db_model::EncryptedBlob& medicationDispense,
                                                     BlobId medicationDispenseBlobId,
                                                     const db_model::HashedTelematikId& telematicId,
                                                     const model::Timestamp& whenHandedOver,
                                                     const std::optional<model::Timestamp>& whenPrepared,
                                                     const db_model::EncryptedBlob& receipt) = 0;
    virtual void updateTaskClearPersonalData(const model::PrescriptionId& taskId, model::Task::Status taskStatus,
                                             const model::Timestamp& lastModified) = 0;

    virtual std::string storeAuditEventData(db_model::AuditData& auditData) = 0;
    virtual std::vector<db_model::AuditData>
    retrieveAuditEventData(const db_model::HashedKvnr& kvnr, const std::optional<Uuid>& id,
                           const std::optional<model::PrescriptionId>& prescriptionId,
                           const std::optional<UrlArguments>& search) = 0;

    virtual std::optional<db_model::Task> retrieveTaskForUpdate(const model::PrescriptionId& taskId) = 0;
    [[nodiscard]] virtual ::std::optional<::db_model::Task>
    retrieveTaskForUpdateAndPrescription(const ::model::PrescriptionId& taskId) = 0;

    virtual std::optional<db_model::Task> retrieveTaskAndReceipt(const model::PrescriptionId& taskId) = 0;
    virtual std::optional<db_model::Task> retrieveTaskAndPrescription(const model::PrescriptionId& taskId) = 0;
    virtual std::optional<db_model::Task> retrieveTaskAndPrescriptionAndReceipt(const model::PrescriptionId& taskId) = 0;
    virtual std::vector<db_model::Task> retrieveAllTasksForPatient(const db_model::HashedKvnr& kvnrHashed,
                                                                   const std::optional<UrlArguments>& search) = 0;
    virtual uint64_t countAllTasksForPatient(const db_model::HashedKvnr& kvnr,
                                             const std::optional<UrlArguments>& search) = 0;

    virtual std::tuple<std::vector<db_model::MedicationDispense>, bool> retrieveAllMedicationDispenses(
        const db_model::HashedKvnr& kvnr,
        const std::optional<model::PrescriptionId>& prescriptionId,
        const std::optional<UrlArguments>& search) = 0;

    virtual CmacKey acquireCmac(const date::sys_days& validDate, const CmacKeyCategory& cmacType, RandomSource& randomSource) = 0;


    /**
     * Insert the `communication` object into the database.
     */
    virtual std::optional<Uuid>
    insertCommunication(const model::PrescriptionId& prescriptionId,
                        const model::Timestamp& timeSent,
                        const model::Communication::MessageType messageType,
                        const db_model::HashedId& sender,
                        const db_model::HashedId& recipient,
                        BlobId senderBlobId,
                        const db_model::EncryptedBlob& messageForSender,
                        BlobId recipientBlobId,
                        const db_model::EncryptedBlob& messageForRecipient) = 0;
    /**
     * Count communication objects between two insurants or representatives, typically the `sender` and the `recipient`
     * of a Communication object, and task with the given `prescriptionId`.
     * Note that sender and recipient can be interchanged, i.e. direction of the communication is ignored.
     */
    virtual uint64_t countRepresentativeCommunications(const db_model::HashedKvnr& insurantA,
                                                       const db_model::HashedKvnr& insurantB,
                                                       const model::PrescriptionId& prescriptionId) = 0;
    /**
      * Checks whether a communication object with the given `communicationId` exists in the database.
      */
    virtual bool existCommunication(const Uuid& communicationId) = 0;
    /**
     * Retrieve all communication objects to or from the given `user` (insurant or pharmacy) with
     * an optional filter on `communicationId` (for getById) and an optional `search` object (for getAll).
     */
    virtual std::vector<db_model::Communication> retrieveCommunications(const db_model::HashedId& user,
                                                                        const std::optional<Uuid>& communicationId,
                                                                        const std::optional<UrlArguments>& search) = 0;
    /**
     * Count communication objects either sent or received by the given user.
     * Note that sender and recipient can be interchanged, i.e. direction of the communication is ignored.
     */
    virtual uint64_t countCommunications(const db_model::HashedId& user,
                                         const std::optional<UrlArguments>& search) = 0;
    /**
     * For tests, retrieve only the database ids of matching communication objects so that these
     * can be counted or deleted.
     */
    virtual std::vector<Uuid> retrieveCommunicationIds(const db_model::HashedId& recipient) = 0;
    /**
     * Tries to remove the communication object defined by its id and sender from the database.
     * If no data row for the specified id and sender has been found the method returns no value.
     * If the data row has been deleted a tuple consisting of the id as the first element and
     * the optional value of the column "received"a as the second element is returned.
     * If "received" does not have a value this means that the communication has not been
     * received by the recipient.
     */
    virtual std::tuple<std::optional<Uuid>, std::optional<model::Timestamp>>
    deleteCommunication(const Uuid& communicationId, const db_model::HashedId& sender) = 0;
    virtual void deleteCommunicationsForTask(const model::PrescriptionId& taskId) = 0;
    virtual void markCommunicationsAsRetrieved(const std::vector<Uuid>& communicationIds,
                                               const model::Timestamp& retrieved,
                                               const db_model::HashedId& recipient) = 0;

    [[nodiscard]]
    virtual std::optional<db_model::Blob> retrieveSaltForAccount(const db_model::HashedId& accountId,
                                                                 db_model::MasterKeyType masterKeyType,
                                                                 BlobId blobId) = 0;

    /// @brief try to insert a new salt into the database
    /// @returns empty if the salt was insert; if the insert failed, returns the salt already contained in the DB
    [[nodiscard]]
    virtual std::optional<db_model::Blob>
    insertOrReturnAccountSalt(const db_model::HashedId& accountId,
                              db_model::MasterKeyType masterKeyType,
                              BlobId blobId,
                              const db_model::Blob& salt) = 0;

    virtual void storeConsent(const db_model::HashedKvnr& kvnr, const model::Timestamp& creationTime) = 0;

    virtual std::optional<model::Timestamp> retrieveConsentDateTime(const db_model::HashedKvnr& kvnr) = 0;
    [[nodiscard]] virtual bool clearConsent(const db_model::HashedKvnr& kvnr) = 0;

    virtual void storeChargeInformation(const ::db_model::ChargeItem& chargeItem, ::db_model::HashedKvnr kvnr) = 0;
    virtual void updateChargeInformation(const ::db_model::ChargeItem& chargeItem) = 0;

    [[nodiscard]] virtual ::std::vector<::db_model::ChargeItem>
    retrieveAllChargeItemsForInsurant(const ::db_model::HashedKvnr& kvnr,
                                      const ::std::optional<UrlArguments>& search) const = 0;

    [[nodiscard]] virtual ::db_model::ChargeItem retrieveChargeInformation(const ::model::PrescriptionId& id) const = 0;
    [[nodiscard]] virtual ::db_model::ChargeItem
    retrieveChargeInformationForUpdate(const ::model::PrescriptionId& id) const = 0;

    virtual void deleteChargeInformation(const model::PrescriptionId& id) = 0;

    virtual void clearAllChargeInformation(const db_model::HashedKvnr& kvnr) = 0;

    virtual void clearAllChargeItemCommunications(const db_model::HashedKvnr& kvnr) = 0;
    virtual void deleteCommunicationsForChargeItem(const model::PrescriptionId& id) = 0;

    virtual uint64_t countChargeInformationForInsurant(const db_model::HashedKvnr& kvnr,
                                             const std::optional<UrlArguments>& search) = 0;
};

#endif//ERP_PROCESSING_CONTEXT_DATABASEBACKEND_HXX
