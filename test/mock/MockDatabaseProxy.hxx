/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_MOCKMOCKDATABASEPROXY_HXX
#define ERP_PROCESSING_CONTEXT_TEST_MOCKMOCKDATABASEPROXY_HXX

#include "erp/database/DatabaseBackend.hxx"
#include "erp/database/DatabaseFrontend.hxx"

class MockDatabase;


/**
 * The SessionContext requires a database factory that returns a unique_ptr to a Database object.
 * In order to keep the MockDatabase persistent during for the duration of a test, and avoid clever
 * management of unique_ptrs, this proxy forwards database calls to a persistent (for a test) database object.
 */
class MockDatabaseProxy : public DatabaseBackend
{
public:
    static Database::Factory createFactory(DatabaseBackend& backend);

    explicit MockDatabaseProxy(DatabaseBackend&);

    void commitTransaction() override;
    void closeConnection() override;

    void healthCheck() override;

    std::tuple<model::PrescriptionId, model::Timestamp> createTask(model::Task::Status taskStatus,
                                                                   const model::Timestamp& lastUpdated,
                                                                   const model::Timestamp& created) override;

    void updateTask(const model::PrescriptionId& taskId,
                    const db_model::EncryptedBlob& accessCode,
                    uint32_t blobId,
                    const db_model::Blob& salt) override;
    /// @returns blobId, salt, authoredOn
    std::tuple<BlobId, db_model::Blob, model::Timestamp>
    getTaskKeyData(const model::PrescriptionId& taskId) override;

    void updateTaskStatusAndSecret(const model::PrescriptionId& taskId,
                                   model::Task::Status status,
                                   const model::Timestamp& lastModifiedDate,
                                   const std::optional<db_model::EncryptedBlob>& secret) override;
    void activateTask(const model::PrescriptionId& taskId,
                      const db_model::EncryptedBlob& encryptedKvnr,
                      const db_model::HashedKvnr& hashedKvnr,
                      model::Task::Status taskStatus,
                      const model::Timestamp& lastModified,
                      const model::Timestamp& expiryDate,
                      const model::Timestamp& acceptDate,
                      const db_model::EncryptedBlob& healthCareProviderPrescription) override;
    void updateTaskMedicationDispenseReceipt(const model::PrescriptionId& taskId,
                                             const model::Task::Status& taskStatus,
                                             const model::Timestamp& lastModified,
                                             const db_model::EncryptedBlob& medicationDispense,
                                             BlobId medicationDispenseBlobId,
                                             const db_model::HashedTelematikId& telematicId,
                                             const model::Timestamp& whenHandedOver,
                                             const std::optional<model::Timestamp>& whenPrepared,
                                             const db_model::EncryptedBlob& receipt) override;
    void updateTaskClearPersonalData(const model::PrescriptionId& taskId,
                                     model::Task::Status taskStatus,
                                     const model::Timestamp& lastModified) override;

    std::string storeAuditEventData(db_model::AuditData& auditData) override;
    std::vector<db_model::AuditData> retrieveAuditEventData(const db_model::HashedKvnr& kvnr,
                                                            const std::optional<Uuid>& id,
                                                            const std::optional<model::PrescriptionId>& prescriptionId,
                                                            const std::optional<UrlArguments>& search) override;
    uint64_t countAuditEventData(const db_model::HashedKvnr& kvnr,
                                 const std::optional<UrlArguments>& search) override;

    std::optional<db_model::Task> retrieveTaskBasics(const model::PrescriptionId& taskId) override;
    std::optional<db_model::Task> retrieveTaskForUpdate(const model::PrescriptionId& taskId) override;
    std::optional<db_model::Task> retrieveTaskAndReceipt(const model::PrescriptionId& taskId) override;
    std::optional<db_model::Task> retrieveTaskAndPrescription(const model::PrescriptionId& taskId) override;
    std::vector<db_model::Task> retrieveAllTasksForPatient(const db_model::HashedKvnr& kvnrHashed,
                                                           const std::optional<UrlArguments>& search) override;
    uint64_t countAllTasksForPatient(const db_model::HashedKvnr& kvnr,
                                     const std::optional<UrlArguments>& search) override;

    std::vector<db_model::MedicationDispense>
    retrieveAllMedicationDispenses(const db_model::HashedKvnr& kvnr,
                                   const std::optional<model::PrescriptionId>& prescriptionId,
                                   const std::optional<UrlArguments>& search) override;
    uint64_t countAllMedicationDispenses(const db_model::HashedKvnr& kvnr,
                                         const std::optional<UrlArguments>& search) override;

    CmacKey acquireCmac(const date::sys_days& validDate, RandomSource& randomSource) override;
    std::optional<Uuid> insertCommunication(const model::PrescriptionId& prescriptionId,
                                            const model::Timestamp& timeSent,
                                            const model::Communication::MessageType messageType,
                                            const db_model::HashedId& sender,
                                            const db_model::HashedId& recipient,
                                            const std::optional<model::Timestamp>& timeReceived,
                                            BlobId senderBlobId,
                                            const db_model::EncryptedBlob& messageForSender,
                                            BlobId recipientBlobId,
                                            const db_model::EncryptedBlob& messageForRecipient) override;
    uint64_t countRepresentativeCommunications(const db_model::HashedKvnr& insurantA,
                                               const db_model::HashedKvnr& insurantB,
                                               const model::PrescriptionId& prescriptionId) override;
    bool existCommunication(const Uuid& communicationId) override;
    std::vector<db_model::Communication> retrieveCommunications(const db_model::HashedId& user,
                                                                const std::optional<Uuid>& communicationId,
                                                                const std::optional<UrlArguments>& search) override;
    uint64_t countCommunications(const db_model::HashedId& user,
                                 const std::optional<UrlArguments>& search) override;

    std::vector<Uuid> retrieveCommunicationIds(const db_model::HashedId& recipient) override;
    std::tuple<std::optional<Uuid>, std::optional<model::Timestamp>>
    deleteCommunication(const Uuid& communicationId, const db_model::HashedId& sender) override;
    void deleteCommunicationsForTask(const model::PrescriptionId& taskId) override;
    void markCommunicationsAsRetrieved(const std::vector<Uuid>& communicationIds,
                                       const model::Timestamp& retrieved,
                                       const db_model::HashedId& recipient) override;
    std::optional<db_model::Blob> insertOrReturnAccountSalt(const db_model::HashedId& accountId,
                                                            db_model::MasterKeyType masterKeyType,
                                                            BlobId blobId,
                                                            const db_model::Blob& salt) override;
    std::optional<db_model::Blob> retrieveSaltForAccount(const db_model::HashedId& accountId,
                                                         db_model::MasterKeyType masterKeyType,
                                                         BlobId blobId) override;
private:
    DatabaseBackend& mDatabase;
};

#endif
