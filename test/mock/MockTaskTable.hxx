/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MOCKTASKTABLE_HXX
#define ERP_PROCESSING_CONTEXT_MOCKTASKTABLE_HXX

#include "erp/database/DatabaseModel.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/Timestamp.hxx"
#include "test/mock/TestUrlArguments.hxx"

#include <map>
#include <optional>
#include <set>

class UrlArguments;
class MockAccountTable;

class MockTaskTable {
public:

    struct Row {
        Row(model::Timestamp initAuthoredOn, model::Timestamp initLastModified);
        model::Timestamp authoredOn;
        model::Timestamp lastModified;
        std::optional<db_model::EncryptedBlob> kvnr = std::nullopt;
        std::optional<db_model::HashedKvnr> kvnrHashed = std::nullopt;
        std::optional<model::Timestamp> expiryDate = std::nullopt;
        std::optional<model::Timestamp> acceptDate = std::nullopt;
        std::optional<model::Task::Status> status = std::nullopt;
        std::optional<int32_t> taskKeyBlobId = std::nullopt;
        std::optional<db_model::Blob> salt = std::nullopt;
        std::optional<db_model::EncryptedBlob> accessCode = std::nullopt;
        std::optional<db_model::EncryptedBlob> secret = std::nullopt;
        std::optional<db_model::EncryptedBlob> healthcareProviderPrescription = std::nullopt;
        std::optional<db_model::EncryptedBlob> receipt = std::nullopt;
        std::optional<model::Timestamp> whenHandedOver = std::nullopt;
        std::optional<model::Timestamp> whenPrepared = std::nullopt;
        std::optional<db_model::HashedTelematikId> performer = std::nullopt;
        std::optional<int32_t> medicationDispenseKeyBlobId = std::nullopt;
        std::optional<db_model::EncryptedBlob> medicationDispenseBundle = std::nullopt;
        std::optional<db_model::EncryptedBlob> chargeItem = std::nullopt;
        std::optional<db_model::EncryptedBlob> dispenseItem = std::nullopt;
        std::optional<model::Timestamp> chargeItemEnteredDate = std::nullopt;
        std::optional<db_model::HashedTelematikId> chargeItemEnterer = std::nullopt;
    };

    explicit MockTaskTable(MockAccountTable& accountTable, const model::PrescriptionType& prescriptionType);

    Row& createRow(const model::PrescriptionId& taskId,
                   const model::Timestamp& lastUpdated,
                   const model::Timestamp& created);

    std::tuple<model::PrescriptionId, model::Timestamp> createTask(model::PrescriptionType prescriptionType,
                                                                   model::Task::Status taskStatus,
                                                                   const model::Timestamp& lastUpdated,
                                                                   const model::Timestamp& created);

    void updateTask(const model::PrescriptionId& taskId,
                    const db_model::EncryptedBlob& accessCode,
                    BlobId blobId,
                    const db_model::Blob& salt);
    /// @returns blobId, salt, authoredOn
    std::tuple<BlobId, db_model::Blob, model::Timestamp>
    getTaskKeyData(const model::PrescriptionId& taskId);

    void updateTaskStatusAndSecret(const model::PrescriptionId& taskId,
                                   model::Task::Status taskStatus,
                                   const model::Timestamp& lastModifiedDate,
                                   const std::optional<db_model::EncryptedBlob>& taskSecret);
    void activateTask(const model::PrescriptionId& taskId,
                      const db_model::EncryptedBlob& encryptedKvnr,
                      const db_model::HashedKvnr& hashedKvnr,
                      model::Task::Status taskStatus,
                      const model::Timestamp& lastModified,
                      const model::Timestamp& expiryDate,
                      const model::Timestamp& acceptDate,
                      const db_model::EncryptedBlob& healthCareProviderPrescription);
    void updateTaskMedicationDispenseReceipt(const model::PrescriptionId& taskId,
                                             const model::Task::Status& taskStatus,
                                             const model::Timestamp& lastModified,
                                             const db_model::EncryptedBlob& medicationDispense,
                                             BlobId medicationDispenseBlobId,
                                             const db_model::HashedTelematikId& telematicId,
                                             const model::Timestamp& whenHandedOver,
                                             const std::optional<model::Timestamp>& whenPrepared,
                                             const db_model::EncryptedBlob& receipt);
    void updateTaskClearPersonalData(const model::PrescriptionId& taskId,
                                     model::Task::Status taskStatus,
                                     const model::Timestamp& lastModified);


    std::optional<db_model::Task> retrieveTaskBasics (const model::PrescriptionId& taskId);
    std::optional<db_model::Task> retrieveTaskAndReceipt(const model::PrescriptionId& taskId);
    ::std::optional<::db_model::Task> retrieveTaskForUpdateAndPrescription(const ::model::PrescriptionId& taskId);
    std::optional<db_model::Task> retrieveTaskAndPrescription(const model::PrescriptionId& taskId);
    std::optional<db_model::Task> retrieveTaskAndPrescriptionAndReceipt(const model::PrescriptionId& taskId);
    std::vector<db_model::Task> retrieveAllTasksForPatient (const db_model::HashedKvnr& kvnrHashed,
                                                            const std::optional<UrlArguments>& search,
                                                            bool applyOnlySearch = false) const;
    uint64_t countAllTasksForPatient(const db_model::HashedKvnr& kvnr,
                                     const std::optional<UrlArguments>& search) const;

    std::vector<TestUrlArguments::SearchMedicationDispense>
    retrieveAllMedicationDispenses(const db_model::HashedKvnr& kvnr,
                                   const std::optional<model::PrescriptionId>& prescriptionId) const;

    void deleteTask(const model::PrescriptionId& taskId);

    void storeChargeInformation(const db_model::HashedTelematikId& pharmacyId, model::PrescriptionId id,
                                const model::Timestamp& enteredDate,
                                const db_model::EncryptedBlob& chargeItem,
                                const db_model::EncryptedBlob& dispenseItem);

    std::vector<db_model::ChargeItem>
    retrieveAllChargeItemsForPharmacy(const db_model::HashedTelematikId& requestingPharmacy,
                                      const std::optional<UrlArguments>& search,
                                      const bool applyPaging = true) const;

    std::vector<db_model::ChargeItem>
    retrieveAllChargeItemsForInsurant(const db_model::HashedKvnr& requestingInsurant,
                                      const std::optional<UrlArguments>& search,
                                      const bool applyPaging = true) const;

    std::tuple<std::optional<db_model::Task>, std::optional<db_model::EncryptedBlob>, std::optional<db_model::EncryptedBlob>>
    retrieveChargeItemAndDispenseItemAndPrescriptionAndReceipt(const model::PrescriptionId& taskId) const;

    std::tuple<db_model::ChargeItem, db_model::EncryptedBlob>
    retrieveChargeInformation(const model::PrescriptionId & id) const;
    std::tuple<db_model::ChargeItem, db_model::EncryptedBlob>
    retrieveChargeInformationForUpdate(const model::PrescriptionId & id) const;

    void deleteChargeInformation(const model::PrescriptionId& id);
    void clearAllChargeInformation(const db_model::HashedKvnr& insurant);

    uint64_t countChargeInformationForInsurant(const db_model::HashedKvnr& kvnr,
                                     const std::optional<UrlArguments>& search) const;
    uint64_t countChargeInformationForPharmacy(const db_model::HashedTelematikId& requestingPharmacy,
                                     const std::optional<UrlArguments>& search) const;
    bool isBlobUsed(BlobId blobId) const;

private:
    enum FieldName
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
        salt,
        access_code,
        secret,
        healthcare_provider_prescription,
        receipt,
        when_handed_over,
        when_prepared,
        performer,
        medication_dispense_key_blob_id,
        medication_dispense_bundle,
    };

    std::optional<db_model::Task> select(int64_t databaseId, const std::set<FieldName>& fields) const;
    void clearChargeInformation(Row& row);


    MockAccountTable& mAccounts;

    int64_t mTaskId = 0;
    std::map<int64_t, Row> mTasks;
    model::PrescriptionType mPrescriptionType;
};


#endif// ERP_PROCESSING_CONTEXT_MOCKTASKTABLE_HXX

