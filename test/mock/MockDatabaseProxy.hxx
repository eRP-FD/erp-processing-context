/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
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

    explicit MockDatabaseProxy(MockDatabase&);

    void commitTransaction() override;
    void closeConnection() override;
    bool isCommitted() const override;

    std::string retrieveSchemaVersion() override;

    void healthCheck() override;

    std::tuple<model::PrescriptionId, model::Timestamp> createTask(model::PrescriptionType prescriptionType,
                                                                   model::Task::Status taskStatus,
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
                                   const std::optional<db_model::EncryptedBlob>& secret,
                                   const std::optional<db_model::EncryptedBlob>& owner) override;
    void activateTask(const model::PrescriptionId& taskId,
                      const db_model::EncryptedBlob& encryptedKvnr,
                      const db_model::HashedKvnr& hashedKvnr,
                      model::Task::Status taskStatus,
                      const model::Timestamp& lastModified,
                      const model::Timestamp& expiryDate,
                      const model::Timestamp& acceptDate,
                      const db_model::EncryptedBlob& healthCareProviderPrescription) override;
    void updateTaskMedicationDispense(const model::PrescriptionId& taskId,
                                      const model::Timestamp& lastModified,
                                      const model::Timestamp& lastMedicationDispense,
                                      const db_model::EncryptedBlob& medicationDispense,
                                      BlobId medicationDispenseBlobId,
                                      const db_model::HashedTelematikId& telematicId,
                                      const model::Timestamp& whenHandedOver,
                                      const std::optional<model::Timestamp>& whenPrepared) override;
    void updateTaskMedicationDispenseReceipt(const model::PrescriptionId& taskId,
                                             const model::Task::Status& taskStatus,
                                             const model::Timestamp& lastModified,
                                             const db_model::EncryptedBlob& medicationDispense,
                                             BlobId medicationDispenseBlobId,
                                             const db_model::HashedTelematikId& telematicId,
                                             const model::Timestamp& whenHandedOver,
                                             const std::optional<model::Timestamp>& whenPrepared,
                                             const db_model::EncryptedBlob& receipt,
                                             const model::Timestamp& lastMedicationDispense) override;
    void updateTaskDeleteMedicationDispense(const model::PrescriptionId& taskId,
                                            const model::Timestamp& lastModified) override;
    void updateTaskClearPersonalData(const model::PrescriptionId& taskId,
                                     model::Task::Status taskStatus,
                                     const model::Timestamp& lastModified) override;

    std::string storeAuditEventData(db_model::AuditData& auditData) override;
    std::vector<db_model::AuditData> retrieveAuditEventData(const db_model::HashedKvnr& kvnr,
                                                            const std::optional<Uuid>& id,
                                                            const std::optional<model::PrescriptionId>& prescriptionId,
                                                            const std::optional<UrlArguments>& search) override;

    std::optional<db_model::Task> retrieveTaskForUpdate(const model::PrescriptionId& taskId) override;
    [[nodiscard]] ::std::optional<::db_model::Task>
    retrieveTaskForUpdateAndPrescription(const ::model::PrescriptionId& taskId) override;
    std::optional<db_model::Task> retrieveTaskAndReceipt(const model::PrescriptionId& taskId) override;
    std::optional<db_model::Task> retrieveTaskAndPrescription(const model::PrescriptionId& taskId) override;
    std::optional<db_model::Task> retrieveTaskWithSecretAndPrescription(const model::PrescriptionId& taskId) override;
    std::optional<db_model::Task> retrieveTaskAndPrescriptionAndReceipt(const model::PrescriptionId& taskId) override;
    std::vector<db_model::Task> retrieveAllTasksForPatient(const db_model::HashedKvnr& kvnrHashed,
                                                           const std::optional<UrlArguments>& search) override;
    std::vector<db_model::Task> retrieveAll160TasksWithAccessCode(const db_model::HashedKvnr& kvnrHashed,
                                                const std::optional<UrlArguments>& search) override;
    uint64_t countAllTasksForPatient(const db_model::HashedKvnr& kvnr,
                                     const std::optional<UrlArguments>& search) override;
    uint64_t countAll160Tasks(const db_model::HashedKvnr& kvnr,
                                     const std::optional<UrlArguments>& search) override;

    std::vector<db_model::MedicationDispense>
    retrieveAllMedicationDispenses(const db_model::HashedKvnr& kvnr,
                                   const std::optional<model::PrescriptionId>& prescriptionId,
                                   const std::optional<UrlArguments>& search) override;

    CmacKey acquireCmac(const date::sys_days& validDate, const CmacKeyCategory& cmacType, RandomSource& randomSource) override;
    std::optional<Uuid> insertCommunication(const model::PrescriptionId& prescriptionId,
                                            const model::Timestamp& timeSent,
                                            const model::Communication::MessageType messageType,
                                            const db_model::HashedId& sender,
                                            const db_model::HashedId& recipient,
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

    void storeConsent(const db_model::HashedKvnr& kvnr, const model::Timestamp& creationTime) override;
    std::optional<model::Timestamp> retrieveConsentDateTime(const db_model::HashedKvnr& kvnr) override;
    [[nodiscard]] bool clearConsent(const db_model::HashedKvnr& kvnr) override;

    void storeChargeInformation(const ::db_model::ChargeItem& chargeItem, ::db_model::HashedKvnr kvnr) override;
    void updateChargeInformation(const ::db_model::ChargeItem& chargeItem) override;

    ::std::vector<::db_model::ChargeItem>
    retrieveAllChargeItemsForInsurant(const ::db_model::HashedKvnr& requestingInsurant,
                                      const ::std::optional<::UrlArguments>& search) const override;

    ::db_model::ChargeItem retrieveChargeInformation(const ::model::PrescriptionId& id) const override;
    ::db_model::ChargeItem retrieveChargeInformationForUpdate(const ::model::PrescriptionId& id) const override;

    void deleteChargeInformation(const ::model::PrescriptionId& id) override;
    void clearAllChargeInformation(const ::db_model::HashedKvnr& kvnr) override;

    void clearAllChargeItemCommunications(const ::db_model::HashedKvnr& kvnr) override;
    void deleteCommunicationsForChargeItem(const model::PrescriptionId& id) override;

    uint64_t countChargeInformationForInsurant(const ::db_model::HashedKvnr& kvnr,
                                               const ::std::optional<::UrlArguments>& search) override;

    bool isBlobUsed(BlobId blobId) const;
    void deleteTask (const model::PrescriptionId& taskId);
    void deleteAuditEvent(const Uuid& eventId);


private:
    MockDatabase& mDatabase;
    class TransactionMonitor
    {
    public:
        TransactionMonitor();
        ~TransactionMonitor() noexcept;
        static thread_local bool inProgress;
    };
    TransactionMonitor transactionMonitor;
};

#endif
