/*
* (C) Copyright IBM Deutschland GmbH 2021, 2025
* (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
*/

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_BACKEND_POSTGRESBACKENDTASK_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_BACKEND_POSTGRESBACKENDTASK_HXX

#include "erp/database/ErpDatabaseBackend.hxx"// for fwd declarations and model includes
#include "shared/database/CommonPostgresBackend.hxx"

#include <pqxx/transaction_base>

class PostgresConnection;
struct TaskQueryIndexes;

class PostgresBackendTask
{
public:

    static constexpr const char* TASK_160_TABLE_NAME = "erp.task";
    static constexpr const char* TASK_162_TABLE_NAME = "erp.task_162";
    static constexpr const char* TASK_166_TABLE_NAME = "erp.task_166";
    static constexpr const char* TASK_169_TABLE_NAME = "erp.task_169";
    static constexpr const char* TASK_200_TABLE_NAME = "erp.task_200";
    static constexpr const char* TASK_209_TABLE_NAME = "erp.task_209";

    // this contains all possible column names of all task tables. They are not necessarily contained in all task tables
    enum class ColumnName
    {
        prescription_id,
        kvnr,
        kvnr_hashed,
        last_modified,
        authored_on,
        expiry_date,
        accept_date,
        status,
        task_key_blob_id,
        prescription_type,
        salt,
        access_code,
        secret,
        healthcare_provider_prescription,
        receipt,
        when_handed_over,
        when_prepared,
        performer,
        medication_dispense_blob_id,
        medication_dispense_bundle,
        owner,
        last_medication_dispense,
        medication_dispense_salt,
        doctor_identity,
        pharmacy_identity,
        last_status_update,
        eu_redeemable,
        eu_redeemable_patient,
        is_pkv,
    };

    explicit PostgresBackendTask(model::PrescriptionType prescriptionType);

    std::tuple<model::PrescriptionId, model::Timestamp>
    createTask(pqxx::transaction_base& transaction, model::Task::Status taskStatus, const model::Timestamp& lastUpdated,
               const model::Timestamp& created, const model::Timestamp& lastStatusUpdate);

    void updateTask(pqxx::transaction_base& transaction, const model::PrescriptionId& taskId,
                    const db_model::EncryptedBlob& accessCode, uint32_t blobId, const db_model::Blob& salt) const;

    std::tuple<BlobId, db_model::Blob, model::Timestamp> getTaskKeyData(pqxx::transaction_base& transaction,
                                                                        const model::PrescriptionId& taskId) const;

    void updateTaskStatusAndSecret(pqxx::transaction_base& transaction, const model::PrescriptionId& taskId,
                                   model::Task::Status status, const model::Timestamp& lastModifiedDate,
                                   const std::optional<db_model::EncryptedBlob>& secret,
                                   const std::optional<db_model::EncryptedBlob>& owner,
                                   const model::Timestamp& lastStatusUpdate) const;

    void activateTask(pqxx::transaction_base& transaction, const model::PrescriptionId& taskId,
                      const db_model::EncryptedBlob& encryptedKvnr, const db_model::HashedKvnr& hashedKvnr,
                      model::Task::Status taskStatus, const model::Timestamp& lastModified,
                      const model::Timestamp& expiryDate, const model::Timestamp& acceptDate,
                      const db_model::EncryptedBlob& healthCareProviderPrescription,
                      const db_model::EncryptedBlob& doctorIdentity, const model::Timestamp& lastStatusUpdate,
                      bool euRedeemable, bool isPkv) const;

    void updateTaskReceipt(pqxx::transaction_base& transaction, const model::PrescriptionId& taskId,
                           const model::Task::Status& taskStatus, const model::Timestamp& lastModified,
                           const db_model::EncryptedBlob& receipt, const db_model::EncryptedBlob& pharmacyIdentity,
                           const model::Timestamp& lastStatusUpdate) const;
    void updateTaskMedicationDispense(pqxx::transaction_base& transaction, const model::PrescriptionId& taskId,
                                      const model::Timestamp& lastModified,
                                      const model::Timestamp& lastMedicationDispense,
                                      const db_model::EncryptedBlob& medicationDispense,
                                      BlobId medicationDispenseBlobId, const db_model::HashedTelematikId& telematikId,
                                      const model::Timestamp& whenHandedOver,
                                      const std::optional<model::Timestamp>& whenPrepared,
                                      const db_model::Blob& medicationDispenseSalt, const std::optional<model::Task::Status>& taskStatus = std::nullopt) const;

    void updateTaskMedicationDispenseReceipt(
        pqxx::transaction_base& transaction, const model::PrescriptionId& taskId, const model::Task::Status& taskStatus,
        const model::Timestamp& lastModified, const db_model::EncryptedBlob& medicationDispense,
        BlobId medicationDispenseBlobId, const db_model::HashedTelematikId& telematikId,
        const model::Timestamp& whenHandedOver, const std::optional<model::Timestamp>& whenPrepared,
        const db_model::EncryptedBlob& receipt, const model::Timestamp& lastMedicationDispense,
        const db_model::Blob& medicationDispenseSalt, const db_model::EncryptedBlob& pharmacyIdentity,
        const model::Timestamp& lastStatusUpdate) const;

    void updateTaskDeleteMedicationDispense(pqxx::transaction_base& transaction, const model::PrescriptionId& taskId,
                                            const model::Timestamp& lastModified) const;

    void updateTaskClearPersonalData(pqxx::transaction_base& transaction, const model::PrescriptionId& taskId,
                                     model::Task::Status taskStatus, const model::Timestamp& lastModified,
                                     const model::Timestamp& lastStatusUpdate) const;

    void updateTaskEuRedeemableByPatient(pqxx::transaction_base& transaction, const model::PrescriptionId& taskId,
                                         bool redeemable,
                                         const model::Timestamp& lastModified) const;

    std::optional<db_model::Task> retrieveTaskForUpdate(pqxx::transaction_base& transaction,
                                                        const model::PrescriptionId& taskId);
    [[nodiscard]] ::std::optional<::db_model::Task>
    retrieveTaskForUpdateAndPrescription(::pqxx::transaction_base& transaction, const ::model::PrescriptionId& taskId);

    [[nodiscard]] std::optional<db_model::Task> retrieveTaskAndReceipt(pqxx::transaction_base& transaction,
                                                                       const model::PrescriptionId& taskId);

    [[nodiscard]] std::optional<db_model::Task> retrieveTaskAndPrescription(pqxx::transaction_base& transaction,
                                                                            const model::PrescriptionId& taskId);

    [[nodiscard]] std::optional<db_model::Task>
    retrieveTaskWithSecretAndPrescription(pqxx::transaction_base& transaction, const model::PrescriptionId& taskId);

    [[nodiscard]] std::optional<db_model::Task>
    retrieveTaskAndPrescriptionAndReceipt(pqxx::transaction_base& transaction, const model::PrescriptionId& taskId);

    [[nodiscard]] std::vector<db_model::Task> retrieveAllTasksWithAccessCode(::pqxx::transaction_base& transaction,
                                                                             const db_model::HashedKvnr& kvnrHashed,
                                                                             const std::optional<UrlArguments>& search);

    [[nodiscard]] static std::vector<db_model::Task> tasksFromQueryResult(const pqxx::result& result,
                                                                          std::optional<model::PrescriptionType> prescriptionType);
    [[nodiscard]] static db_model::Task taskFromQueryResultRow(const pqxx::row& resultRow,
                                                               std::optional<model::PrescriptionType> prescriptionType);
    [[nodiscard]] static db_model::Task taskFromQueryResultRow(const pqxx::row& resultRow,
                                                               std::optional<model::PrescriptionType> prescriptionType,
                                                               const TaskQueryIndexes& indexes);

private:
    std::string taskTableName() const;

    struct Queries {
        QueryDefinition createTask;
        QueryDefinition updateTask;
        QueryDefinition updateTask_secret;
        QueryDefinition updateTask_medicationDispense;
        QueryDefinition updateTask_medicationDispenseAndStatus;
        QueryDefinition updateTask_medicationDispenseReceipt;
        QueryDefinition updateTask_activateTask;
        QueryDefinition updateTask_receipt;
        QueryDefinition updateTask_deleteMedicationDispense;
        QueryDefinition updateTask_deletePersonalData;
        QueryDefinition updateTask_storeChargeInformation;
        QueryDefinition updateTaskMarkingFlag;
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
