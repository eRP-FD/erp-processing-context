/*
* (C) Copyright IBM Deutschland GmbH 2021, 2024
* (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
*/

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_BACKEND_POSTGRESBACKENDTASK_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_BACKEND_POSTGRESBACKENDTASK_HXX

#include "erp/database/ErpDatabaseBackend.hxx"// for fwd declarations and model includes
#include "shared/database/CommonPostgresBackend.hxx"

#include <pqxx/transaction>

class PostgresConnection;

class PostgresBackendTask
{
public:

    static constexpr const char* TASK_160_TABLE_NAME = "erp.task";
    static constexpr const char* TASK_169_TABLE_NAME = "erp.task_169";
    static constexpr const char* TASK_200_TABLE_NAME = "erp.task_200";
    static constexpr const char* TASK_209_TABLE_NAME = "erp.task_209";

    explicit PostgresBackendTask(model::PrescriptionType prescriptionType);

    std::tuple<model::PrescriptionId, model::Timestamp> createTask(pqxx::work& transaction,
                                                                   model::Task::Status taskStatus,
                                                                   const model::Timestamp& lastUpdated,
                                                                   const model::Timestamp& created,
                                                                   const model::Timestamp& lastStatusUpdate);

    void updateTask(pqxx::work& transaction, const model::PrescriptionId& taskId,
                    const db_model::EncryptedBlob& accessCode, uint32_t blobId, const db_model::Blob& salt) const;

    std::tuple<BlobId, db_model::Blob, model::Timestamp> getTaskKeyData(pqxx::work& transaction,
                                                                        const model::PrescriptionId& taskId) const;

    void updateTaskStatusAndSecret(pqxx::work& transaction, const model::PrescriptionId& taskId,
                                   model::Task::Status status, const model::Timestamp& lastModifiedDate,
                                   const std::optional<db_model::EncryptedBlob>& secret,
                                   const std::optional<db_model::EncryptedBlob>& owner,
                                   const model::Timestamp& lastStatusUpdate) const;

    void activateTask(pqxx::work& transaction, const model::PrescriptionId& taskId,
                      const db_model::EncryptedBlob& encryptedKvnr, const db_model::HashedKvnr& hashedKvnr,
                      model::Task::Status taskStatus, const model::Timestamp& lastModified,
                      const model::Timestamp& expiryDate, const model::Timestamp& acceptDate,
                      const db_model::EncryptedBlob& healthCareProviderPrescription,
                      const db_model::EncryptedBlob& doctorIdentity,
                      const model::Timestamp& lastStatusUpdate) const;

    void updateTaskReceipt(pqxx::work& transaction, const model::PrescriptionId& taskId,
                           const model::Task::Status& taskStatus, const model::Timestamp& lastModified,
                           const db_model::EncryptedBlob& receipt,
                           const db_model::EncryptedBlob& pharmacyIdentity,
                           const model::Timestamp& lastStatusUpdate) const;
    void updateTaskMedicationDispense(
        pqxx::work& transaction,
        const model::PrescriptionId& taskId,
        const model::Timestamp& lastModified,
        const model::Timestamp& lastMedicationDispense,
        const db_model::EncryptedBlob& medicationDispense,
        BlobId medicationDispenseBlobId,
        const db_model::HashedTelematikId& telematikId,
        const model::Timestamp& whenHandedOver,
        const std::optional<model::Timestamp>& whenPrepared,
        const db_model::Blob& medicationDispenseSalt) const;

    void updateTaskMedicationDispenseReceipt(
        pqxx::work& transaction, const model::PrescriptionId& taskId, const model::Task::Status& taskStatus,
        const model::Timestamp& lastModified, const db_model::EncryptedBlob& medicationDispense,
        BlobId medicationDispenseBlobId, const db_model::HashedTelematikId& telematikId,
        const model::Timestamp& whenHandedOver, const std::optional<model::Timestamp>& whenPrepared,
        const db_model::EncryptedBlob& receipt, const model::Timestamp& lastMedicationDispense,
        const db_model::Blob& medicationDispenseSalt, const db_model::EncryptedBlob& pharmacyIdentity,
        const model::Timestamp& lastStatusUpdate) const;

    void updateTaskDeleteMedicationDispense(
        pqxx::work& transaction, const model::PrescriptionId& taskId, const model::Timestamp& lastModified) const;

    void updateTaskClearPersonalData(pqxx::work& transaction, const model::PrescriptionId& taskId,
                                     model::Task::Status taskStatus, const model::Timestamp& lastModified,
                                     const model::Timestamp& lastStatusUpdate) const;

    std::optional<db_model::Task> retrieveTaskForUpdate(pqxx::work& transaction, const model::PrescriptionId& taskId);
    [[nodiscard]] ::std::optional<::db_model::Task>
    retrieveTaskForUpdateAndPrescription(::pqxx::work& transaction, const ::model::PrescriptionId& taskId);

    [[nodiscard]] std::optional<db_model::Task> retrieveTaskAndReceipt(pqxx::work& transaction,
                                                                       const model::PrescriptionId& taskId);

    [[nodiscard]] std::optional<db_model::Task>
    retrieveTaskAndPrescription(pqxx::work& transaction, const model::PrescriptionId& taskId);

    [[nodiscard]] std::optional<db_model::Task>
    retrieveTaskWithSecretAndPrescription(pqxx::work& transaction, const model::PrescriptionId& taskId);

    [[nodiscard]] std::optional<db_model::Task>
    retrieveTaskAndPrescriptionAndReceipt(pqxx::work& transaction, const model::PrescriptionId& taskId);

    [[nodiscard]] std::vector<db_model::Task> retrieveAllTasksWithAccessCode(::pqxx::work& transaction,
                                                                             const db_model::HashedKvnr& kvnrHashed,
                                                                             const std::optional<UrlArguments>& search);

    struct TaskQueryIndexes {
        pqxx::row::size_type prescriptionIdIndex = 0;
        pqxx::row::size_type kvnrIndex = 1;
        pqxx::row::size_type lastModifiedIndex = 2;
        pqxx::row::size_type authoredOnIndex = 3;
        pqxx::row::size_type expiryDateIndex = 4;
        pqxx::row::size_type acceptDateIndex = 5;
        pqxx::row::size_type statusIndex = 6;
        pqxx::row::size_type lastStatusChangeIndex = 7;
        pqxx::row::size_type saltIndex = 8;
        pqxx::row::size_type keyBlobIdIndex = 9;
        std::optional<pqxx::row::size_type> accessCodeIndex = 10;
        std::optional<pqxx::row::size_type> secretIndex = 11;
        std::optional<pqxx::row::size_type> ownerIndex = 12;
        std::optional<pqxx::row::size_type> healthcareProviderPrescriptionIndex = {};
        std::optional<pqxx::row::size_type> receiptIndex = {};
        std::optional<pqxx::row::size_type> lastMedicationDispenseIndex = {};
    };

    [[nodiscard]] static db_model::Task taskFromQueryResultRow(const pqxx::row& resultRow, const TaskQueryIndexes& indexes,
                                                               model::PrescriptionType prescriptionType);

private:
    std::string taskTableName() const;

    struct Queries {
        QueryDefinition createTask;
        QueryDefinition updateTask;
        QueryDefinition updateTask_secret;
        QueryDefinition updateTask_medicationDispense;
        QueryDefinition updateTask_medicationDispenseReceipt;
        QueryDefinition updateTask_activateTask;
        QueryDefinition updateTask_receipt;
        QueryDefinition updateTask_deleteMedicationDispense;
        QueryDefinition updateTask_deletePersonalData;
        QueryDefinition updateTask_storeChargeInformation;
        QueryDefinition retrieveTaskById;
        QueryDefinition retrieveTaskByIdPlusReceipt;
        QueryDefinition retrieveTaskByIdForUpdatePlusPrescription;
        QueryDefinition retrieveTaskByIdPlusPrescription;
        QueryDefinition retrieveTaskWithSecretByIdPlusPrescription;
        QueryDefinition retrieveTaskByIdPlusPrescriptionPlusReceipt;
        QueryDefinition retrieveAllTasksByKvnrWithAccessCode;
        QueryDefinition getTaskKeyData;
    };
    Queries mQueries;
    model::PrescriptionType mPrescriptionType;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_BACKEND_POSTGRESBACKENDTASK_HXX
