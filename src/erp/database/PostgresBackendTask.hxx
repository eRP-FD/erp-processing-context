/*
* (C) Copyright IBM Deutschland GmbH 2021
* (C) Copyright IBM Corp. 2021
*/

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_BACKEND_POSTGRESBACKENDTASK_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_BACKEND_POSTGRESBACKENDTASK_HXX

#include "erp/database/DatabaseBackend.hxx"// for fwd declarations and model includes
#include "erp/database/PostgresBackendHelper.hxx"

#include <pqxx/transaction>

class PostgresConnection;

class PostgresBackendTask
{
public:

    static constexpr const char* TASK_160_TABLE_NAME = "erp.task";
    static constexpr const char* TASK_169_TABLE_NAME = "erp.task_169";
    static constexpr const char* TASK_200_TABLE_NAME = "erp.task_200";

    explicit PostgresBackendTask(model::PrescriptionType prescriptionType);

    std::tuple<model::PrescriptionId, model::Timestamp> createTask(pqxx::work& transaction,
                                                                   model::Task::Status taskStatus,
                                                                   const model::Timestamp& lastUpdated,
                                                                   const model::Timestamp& created);

    void updateTask(pqxx::work& transaction, const model::PrescriptionId& taskId,
                    const db_model::EncryptedBlob& accessCode, uint32_t blobId, const db_model::Blob& salt);

    std::tuple<BlobId, db_model::Blob, model::Timestamp> getTaskKeyData(pqxx::work& transaction,
                                                                        const model::PrescriptionId& taskId);

    void updateTaskStatusAndSecret(pqxx::work& transaction, const model::PrescriptionId& taskId,
                                   model::Task::Status status, const model::Timestamp& lastModifiedDate,
                                   const std::optional<db_model::EncryptedBlob>& secret);

    void activateTask(pqxx::work& transaction, const model::PrescriptionId& taskId,
                      const db_model::EncryptedBlob& encryptedKvnr, const db_model::HashedKvnr& hashedKvnr,
                      model::Task::Status taskStatus, const model::Timestamp& lastModified,
                      const model::Timestamp& expiryDate, const model::Timestamp& acceptDate,
                      const db_model::EncryptedBlob& healthCareProviderPrescription);

    void updateTaskMedicationDispenseReceipt(
        pqxx::work& transaction, const model::PrescriptionId& taskId, const model::Task::Status& taskStatus,
        const model::Timestamp& lastModified, const db_model::EncryptedBlob& medicationDispense,
        BlobId medicationDispenseBlobId, const db_model::HashedTelematikId& telematikId,
        const model::Timestamp& whenHandedOver, const std::optional<model::Timestamp>& whenPrepared,
        const db_model::EncryptedBlob& receipt);

    void updateTaskClearPersonalData(pqxx::work& transaction, const model::PrescriptionId& taskId,
                                     model::Task::Status taskStatus, const model::Timestamp& lastModified);

    std::optional<db_model::Task> retrieveTaskForUpdate(pqxx::work& transaction, const model::PrescriptionId& taskId);
    [[nodiscard]] ::std::optional<::db_model::Task>
    retrieveTaskForUpdateAndPrescription(::pqxx::work& transaction, const ::model::PrescriptionId& taskId);

    [[nodiscard]] std::optional<db_model::Task> retrieveTaskAndReceipt(pqxx::work& transaction,
                                                                       const model::PrescriptionId& taskId);

    [[nodiscard]] std::optional<db_model::Task>
    retrieveTaskAndPrescription(pqxx::work& transaction, const model::PrescriptionId& taskId);

    [[nodiscard]] std::optional<db_model::Task>
    retrieveTaskAndPrescriptionAndReceipt(pqxx::work& transaction, const model::PrescriptionId& taskId);

    [[nodiscard]] uint64_t countAllTasksForPatient(pqxx::work& transaction, const db_model::HashedKvnr& kvnr,
                                                   const std::optional<UrlArguments>& search);

    void storeChargeInformation(::pqxx::work& transaction, const db_model::HashedTelematikId& pharmacyTelematikId,
                                model::PrescriptionId id, const model::Timestamp& enteredDate,
                                const db_model::EncryptedBlob& chargeItem, const db_model::EncryptedBlob& dispenseItem);

    [[nodiscard]] std::vector<db_model::ChargeItem>
    retrieveAllChargeItemsForInsurant(::pqxx::work& transaction, const db_model::HashedKvnr& kvnr,
                                      const std::optional<UrlArguments>& search) const;

    [[nodiscard]] std::vector<db_model::ChargeItem>
    retrieveAllChargeItemsForPharmacy(::pqxx::work& transaction, const db_model::HashedTelematikId& pharmacyTelematikId,
                                      const std::optional<UrlArguments>& search) const;

    [[nodiscard]] std::tuple<db_model::ChargeItem, db_model::EncryptedBlob>
    retrieveChargeInformation(::pqxx::work& transaction, const model::PrescriptionId& id) const;
    [[nodiscard]] std::tuple<db_model::ChargeItem, db_model::EncryptedBlob>
    retrieveChargeInformationForUpdate(::pqxx::work& transaction, const model::PrescriptionId& id) const;

    void deleteChargeInformation(::pqxx::work& transaction, const model::PrescriptionId& id);
    void clearAllChargeInformation(::pqxx::work& transaction, const db_model::HashedKvnr& kvnr);

    [[nodiscard]] uint64_t countChargeInformationForInsurant(pqxx::work& transaction,
                                                             const db_model::HashedKvnr& kvnr,
                                                             const std::optional<UrlArguments>& search);
    [[nodiscard]] uint64_t countChargeInformationForPharmacy(pqxx::work& transaction,
                                                             const db_model::HashedTelematikId& pharmacyTelematikId,
                                                             const std::optional<UrlArguments>& search);

    struct TaskQueryIndexes {
        pqxx::row::size_type prescriptionIdIndex = 0;
        pqxx::row::size_type kvnrIndex = 1;
        pqxx::row::size_type lastModifiedIndex = 2;
        pqxx::row::size_type authoredOnIndex = 3;
        pqxx::row::size_type expiryDateIndex = 4;
        pqxx::row::size_type acceptDateIndex = 5;
        pqxx::row::size_type statusIndex = 6;
        pqxx::row::size_type saltIndex = 7;
        pqxx::row::size_type keyBlobIdIndex = 8;
        std::optional<pqxx::row::size_type> accessCodeIndex = 9;
        std::optional<pqxx::row::size_type> secretIndex = 10;
        std::optional<pqxx::row::size_type> healthcareProviderPrescriptionIndex = {};
        std::optional<pqxx::row::size_type> receiptIndex = {};
    };

    [[nodiscard]] static db_model::Task taskFromQueryResultRow(const pqxx::row& resultRow, const TaskQueryIndexes& indexes,
                                                               model::PrescriptionType prescriptionType);

private:
    std::string taskTableName() const;
    [[nodiscard]] std::vector<db_model::ChargeItem>
    retrieveAllChargeItems(pqxx::work& transaction, const QueryDefinition& baseQuery, const db_model::HashedId& Id,
                           const std::optional<UrlArguments>& search) const;
    [[nodiscard]] db_model::ChargeItem chargeItemFromQueryResultRow(const pqxx::row& row) const;


    struct Queries {
        QueryDefinition clearAllChargeInformation;
        QueryDefinition createTask;
        QueryDefinition deleteChargeInformation;
        QueryDefinition updateTask;
        QueryDefinition updateTask_secret;
        QueryDefinition updateTask_medicationDispenseReceipt;
        QueryDefinition updateTask_activateTask;
        QueryDefinition updateTask_deletePersonalData;
        QueryDefinition updateTask_storeChargeInformation;
        QueryDefinition retrieveAllChargeItemsForInsurant;
        QueryDefinition retrieveAllChargeItemsForPharmacy;
        QueryDefinition retrieveChargeInformation;
        QueryDefinition retrieveTaskById;
        QueryDefinition retrieveTaskByIdPlusReceipt;
        QueryDefinition retrieveTaskByIdForUpdatePlusPrescription;
        QueryDefinition retrieveTaskByIdPlusPrescription;
        QueryDefinition retrieveTaskByIdPlusPrescriptionPlusReceipt;
        QueryDefinition countAllTasksByKvnr;
        QueryDefinition getTaskKeyData;
        QueryDefinition countChargeInformationForInsurant;
        QueryDefinition countChargeInformationForPharmacy;
    };
    Queries mQueries;
    model::PrescriptionType mPrescriptionType;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_BACKEND_POSTGRESBACKENDTASK_HXX
