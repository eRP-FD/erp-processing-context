/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_DATABASE_POSTGRESBACKEND_HXX
#define ERP_PROCESSING_CONTEXT_DATABASE_POSTGRESBACKEND_HXX

#include "erp/database/ErpDatabaseBackend.hxx"
#include "erp/database/PostgresBackendChargeItem.hxx"
#include "erp/database/PostgresBackendTask.hxx"
#include "shared/database/CommonPostgresBackend.hxx"
#include "shared/database/PostgresConnection.hxx"

#include <memory>
#include <pqxx/binarystring>
#include <pqxx/transaction_base>

namespace pqxx {class connection;}

class PostgresBackend : public CommonPostgresBackend, public ErpDatabaseBackend
{
public:
    PostgresBackend (void);
    ~PostgresBackend (void) override;

    static void recreateConnection();

    void healthCheck() override;

    [[nodiscard]] static uint64_t executeCountQuery(pqxx::transaction_base& transaction, const std::string_view& query,
                                                    const db_model::Blob& paramValue,
                                                    const std::optional<UrlArguments>& search,
                                                    const std::string_view& context);
    std::tuple<model::PrescriptionId, model::Timestamp> createTask(model::PrescriptionType prescriptionType,
                                                                   model::Task::Status taskStatus,
                                                                   const model::Timestamp& lastUpdated,
                                                                   const model::Timestamp& created,
                                                                   const model::Timestamp& lastStatusUpdate) override;

    void updateTask(const model::PrescriptionId& taskId,
                    const db_model::EncryptedBlob& accessCode,
                    uint32_t blobId,
                    const db_model::Blob& salt) override;

    std::tuple<BlobId, db_model::Blob, model::Timestamp>
    getTaskKeyData(const model::PrescriptionId & taskId) override;

    void updateTaskStatusAndSecret(const model::PrescriptionId& taskId,
                                   model::Task::Status status,
                                   const model::Timestamp& lastModifiedDate,
                                   const std::optional<db_model::EncryptedBlob>& secret,
                                   const std::optional<db_model::EncryptedBlob>& owner,
                                   const model::Timestamp& lastStatusUpdate) override;
    void activateTask(const model::PrescriptionId& taskId,
                      const db_model::EncryptedBlob& encryptedKvnr,
                      const db_model::HashedKvnr& hashedKvnr,
                      model::Task::Status taskStatus,
                      const model::Timestamp& lastModified,
                      const model::Timestamp& expiryDate,
                      const model::Timestamp& acceptDate,
                      const db_model::EncryptedBlob& healthCareProviderPrescription,
                      const db_model::EncryptedBlob& doctorIdentity,
                      const model::Timestamp& lastStatusUpdate) override;
    void updateTaskReceipt(const model::PrescriptionId& taskId, const model::Task::Status& taskStatus,
                           const model::Timestamp& lastModified, const db_model::EncryptedBlob& receipt,
                           const db_model::EncryptedBlob& pharmacyIdentity,
                           const model::Timestamp& lastStatusUpdate) override;
    void updateTaskMedicationDispense(const model::PrescriptionId& taskId,
                                      const model::Timestamp& lastModified,
                                      const model::Timestamp& lastMedicationDispense,
                                      const db_model::EncryptedBlob& medicationDispense,
                                      BlobId medicationDispenseBlobId,
                                      const db_model::HashedTelematikId& telematikId,
                                      const model::Timestamp& whenHandedOver,
                                      const std::optional<model::Timestamp>& whenPrepared,
                                      const db_model::Blob& medicationDispenseSalt) override;
    void updateTaskMedicationDispenseReceipt(
        const model::PrescriptionId& taskId, const model::Task::Status& taskStatus,
        const model::Timestamp& lastModified, const db_model::EncryptedBlob& medicationDispense,
        BlobId medicationDispenseBlobId, const db_model::HashedTelematikId& telematikId,
        const model::Timestamp& whenHandedOver, const std::optional<model::Timestamp>& whenPrepared,
        const db_model::EncryptedBlob& receipt, const model::Timestamp& lastMedicationDispense,
        const db_model::Blob& medicationDispenseSalt, const db_model::EncryptedBlob& pharmacyIdentity,
        const model::Timestamp& lastStatusUpdate) override;
    void updateTaskDeleteMedicationDispense(const model::PrescriptionId& taskId,
                                            const model::Timestamp& lastModified) override;
    void updateTaskClearPersonalData(const model::PrescriptionId& taskId,
                                     model::Task::Status taskStatus,
                                     const model::Timestamp& lastModified,
                                     const model::Timestamp& lastStatusUpdate) override;

    [[nodiscard]]
    std::vector<db_model::AuditData> retrieveAuditEventData(
        const db_model::HashedKvnr& kvnr,
        const std::optional<Uuid>& id,
        const std::optional<model::PrescriptionId>& prescriptionId,
        const std::optional<UrlArguments>& search) override;

    [[nodiscard]]
    std::optional<db_model::Task> retrieveTaskForUpdate(const model::PrescriptionId& taskId) override;
    [[nodiscard]] ::std::optional<::db_model::Task>
    retrieveTaskForUpdateAndPrescription(const ::model::PrescriptionId& taskId) override;

    [[nodiscard]]
    std::optional<db_model::Task> retrieveTaskAndReceipt(const model::PrescriptionId& taskId) override;
    [[nodiscard]]
    std::optional<db_model::Task> retrieveTaskAndPrescription(const model::PrescriptionId& taskId) override;
    [[nodiscard]]
    std::optional<db_model::Task> retrieveTaskWithSecretAndPrescription(const model::PrescriptionId& taskId) override;
    [[nodiscard]]
    std::optional<db_model::Task> retrieveTaskAndPrescriptionAndReceipt(const model::PrescriptionId& taskId) override;
    [[nodiscard]] std::vector<db_model::Task>
    retrieveAllTasksForPatient(const db_model::HashedKvnr& kvnrHashed,
                               const std::optional<UrlArguments>& search) override;
    [[nodiscard]] std::vector<db_model::Task>
    retrieveAll160TasksWithAccessCode(const db_model::HashedKvnr& kvnrHashed,
                                      const std::optional<UrlArguments>& search) override;
    [[nodiscard]] uint64_t countAllTasksForPatient(const db_model::HashedKvnr& kvnr, const std::optional<UrlArguments>& search) override;
    [[nodiscard]] uint64_t countAll160Tasks(const db_model::HashedKvnr& kvnr, const std::optional<UrlArguments>& search) override;

    [[nodiscard]]
    std::vector<db_model::MedicationDispense> retrieveAllMedicationDispenses(
        const db_model::HashedKvnr& kvnr,
        const std::optional<model::PrescriptionId>& prescriptionId,
        const std::optional<UrlArguments>& search) override;

    [[nodiscard]]
    CmacKey acquireCmac(const date::sys_days& validDate, const CmacKeyCategory& cmacType, RandomSource& randomSource) override;
    [[nodiscard]]
    std::optional<Uuid> insertCommunication(const model::PrescriptionId& prescriptionId,
                                            const model::Timestamp& timeSent,
                                            const model::Communication::MessageType messageType,
                                            const db_model::HashedId& sender,
                                            const db_model::HashedId& recipient,
                                            BlobId senderBlobId,
                                            const db_model::EncryptedBlob& messageForSender,
                                            BlobId recipientBlobId,
                                            const db_model::EncryptedBlob& messageForRecipient) override;
    [[nodiscard]]
    uint64_t countRepresentativeCommunications(
        const db_model::HashedKvnr& insurantA,
        const db_model::HashedKvnr& insurantB,
        const model::PrescriptionId& prescriptionId) override;
    bool existCommunication(const Uuid& communicationId) override;
    std::vector<db_model::Communication> retrieveCommunications (
        const db_model::HashedId& user,
        const std::optional<Uuid>& communicationId,
        const std::optional<UrlArguments>& search) override;
    [[nodiscard]] uint64_t countCommunications(
        const db_model::HashedId& user,
        const std::optional<UrlArguments>& search) override;

    std::vector<Uuid> retrieveCommunicationIds (const db_model::HashedId& recipient) override;

    std::tuple<std::optional<Uuid>, std::optional<model::Timestamp>>
    deleteCommunication(const Uuid& communicationId, const db_model::HashedId& sender) override;

    void markCommunicationsAsRetrieved (
        const std::vector<Uuid>& communicationIds,
        const model::Timestamp& retrieved,
        const db_model::HashedId& recipient) override;
    void deleteCommunicationsForTask (const model::PrescriptionId& taskId) override;

    void storeConsent(const db_model::HashedKvnr& kvnr, const model::Timestamp& creationTime) override;
    std::optional<model::Timestamp> retrieveConsentDateTime(const db_model::HashedKvnr& kvnr) override;
    [[nodiscard]] bool clearConsent(const db_model::HashedKvnr& kvnr) override;

    void storeChargeInformation(const ::db_model::ChargeItem& chargeItem, ::db_model::HashedKvnr kvnr) override;
    void updateChargeInformation(const ::db_model::ChargeItem& chargeItem) override;

    [[nodiscard]] ::std::vector<::db_model::ChargeItem>
    retrieveAllChargeItemsForInsurant(const ::db_model::HashedKvnr& kvnr,
                                      const ::std::optional<UrlArguments>& search) const override;

    [[nodiscard]] ::db_model::ChargeItem retrieveChargeInformation(const ::model::PrescriptionId& id) const override;
    [[nodiscard]] ::db_model::ChargeItem
    retrieveChargeInformationForUpdate(const ::model::PrescriptionId& id) const override;

    void deleteChargeInformation(const model::PrescriptionId& id) override;
    void clearAllChargeInformation(const db_model::HashedKvnr& kvnr) override;
    void clearAllChargeItemCommunications(const db_model::HashedKvnr& kvnr) override;
    void deleteCommunicationsForChargeItem(const model::PrescriptionId& taskId) override;

    [[nodiscard]] uint64_t countChargeInformationForInsurant(const db_model::HashedKvnr& kvnr,
                                                             const std::optional<UrlArguments>& search) override;

    PostgresConnection& connection() const override;
private:
    PostgresBackendTask& getTaskBackend(const model::PrescriptionType prescriptionType);

    thread_local static PostgresConnection mConnection;

    PostgresBackendTask mBackendTask;
    PostgresBackendTask mBackendTask162;
    PostgresBackendTask mBackendTask169;
    PostgresBackendTask mBackendTask200;
    PostgresBackendTask mBackendTask209;
    PostgresBackendChargeItem mBackendChargeItem = {};
};

#endif // ERP_PROCESSING_CONTEXT_DATABASE_POSTGRESBACKEND_HXX
