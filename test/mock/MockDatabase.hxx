/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_MOCK_MOCKDATABASE_HXX
#define ERP_PROCESSING_CONTEXT_TEST_MOCK_MOCKDATABASE_HXX

#include "erp/database/ErpDatabaseBackend.hxx"
#include "erp/model/Communication.hxx"
#include "shared/model/Binary.hxx"
#include "shared/crypto/CMAC.hxx"
#include "shared/database/DatabaseCodec.hxx"
#include "shared/database/DatabaseModel.hxx"
#include "shared/hsm/KeyDerivation.hxx"
#include "shared/model/AuditData.hxx"
#include "shared/model/Timestamp.hxx"
#include "test/mock/MockAccountTable.hxx"
#include "test/mock/MockChargeItemTable.hxx"
#include "test/mock/MockCommunicationTable.hxx"
#include "test/mock/MockConsentTable.hxx"
#include "test/mock/MockTaskTable.hxx"

#include <list>
#include <map>
#include <mutex>

class Database;

class MockDatabase : public ErpDatabaseBackend
{
public:
    static constexpr std::string_view mockAccessCode = "b79e5bca8b072113f08c43ce22aa1dded4db61ef21571b37911b6dfc852004f6";

    static std::unique_ptr<Database> createMockDatabase(HsmPool& hsmPool, KeyDerivation& keyDerivation);

    explicit MockDatabase(HsmPool& hsmPool);

    void commitTransaction() override;
    void closeConnection() override;
    bool isCommitted() const override;

    std::string retrieveSchemaVersion() override;

    void healthCheck() override;

    void fillWithStaticData();

    std::tuple<model::PrescriptionId, model::Timestamp> createTask(model::PrescriptionType prescriptionType,
                                                                   model::Task::Status taskStatus,
                                                                   const model::Timestamp& lastUpdated,
                                                                   const model::Timestamp& created,
                                                                   const model::Timestamp& lastStatusUpdate) override;

    void updateTask(const model::PrescriptionId& taskId,
                    const db_model::EncryptedBlob& accessCode,
                    uint32_t blobId,
                    const db_model::Blob& salt) override;
    /// @returns blobId, salt, authoredOn
    std::tuple<BlobId, db_model::Blob, model::Timestamp>
    getTaskKeyData(const model::PrescriptionId& taskId) override;

    void updateTaskStatusAndSecret(const model::PrescriptionId& taskId,
                                   model::Task::Status taskStatus,
                                   const model::Timestamp& lastModifiedDate,
                                   const std::optional<db_model::EncryptedBlob>& taskSecret,
                                   const std::optional<db_model::EncryptedBlob>& owner,
                                   const model::Timestamp& lastStatusUpdate) override;
    void activateTask(const model::PrescriptionId& taskId, const db_model::EncryptedBlob& encryptedKvnr,
                      const db_model::HashedKvnr& hashedKvnr, model::Task::Status taskStatus,
                      const model::Timestamp& lastModified, const model::Timestamp& expiryDate,
                      const model::Timestamp& acceptDate, const db_model::EncryptedBlob& healthCareProviderPrescription,
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
        BlobId medicationDispenseBlobId, const db_model::HashedTelematikId& telematicId,
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

    std::string storeAuditEventData(db_model::AuditData& auditData) override;
    std::vector<db_model::AuditData> retrieveAuditEventData(
        const db_model::HashedKvnr& kvnr,
        const std::optional<Uuid>& id,
        const std::optional<model::PrescriptionId>& prescriptionId,
        const std::optional<UrlArguments>& search) override;

    std::optional<db_model::Task> retrieveTaskForUpdate (const model::PrescriptionId& taskId) override;
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
    retrieveAllMedicationDispenses (const db_model::HashedKvnr& kvnr,
                                    const std::optional<model::PrescriptionId>& prescriptionId,
                                    const std::optional<UrlArguments>& search) override;

    CmacKey acquireCmac(const date::sys_days& validDate, const CmacKeyCategory& cmacType, RandomSource& randomSource) override;

    /**
     * Insert the `communication` object into the database.
     */
    std::optional<Uuid> insertCommunication(const model::PrescriptionId& prescriptionId,
                                            const model::Timestamp& timeSent,
                                            const model::Communication::MessageType messageType,
                                            const db_model::HashedId& sender,
                                            const db_model::HashedId& recipient,
                                            BlobId senderBlobId,
                                            const db_model::EncryptedBlob& messageForSender,
                                            BlobId recipientBlobId,
                                            const db_model::EncryptedBlob& messageForRecipient) override;
    /**
     * Count communication objects between two insurants or representatives, typically the `sender` and the `recipient`
     * of a Communication object, and task with the given `prescriptionId`.
     * Note that sender and recipient can be interchanged, i.e. direction of the communication is ignored.
     */
    uint64_t countRepresentativeCommunications(
        const db_model::HashedKvnr& insurantA,
        const db_model::HashedKvnr& insurantB,
        const model::PrescriptionId& prescriptionId) override;
    /**
      * Checks whether a communication object with the given `communicationId` exists in the database.
      */
    bool existCommunication(const Uuid& communicationId) override;
    /**
     * Retrieve all communication objects to or from the given `user` (insurant or pharmacy) with
     * an optional filter on `communicationId` (for getById) and an optional `search` object (for getAll).
     */
    std::vector<db_model::Communication> retrieveCommunications (
        const db_model::HashedId& user,
        const std::optional<Uuid>& communicationId,
        const std::optional<UrlArguments>& search) override;
    /**
     * Count communication objects either sent or received by the given user.
     * Note that sender and recipient can be interchanged, i.e. direction of the communication is ignored.
     */
    uint64_t countCommunications(
        const db_model::HashedId& user,
        const std::optional<UrlArguments>& search) override;

    /**
     * For tests, retrieve only the database ids of matching communication objects so that these
     * can be counted or deleted.
     */
    std::vector<Uuid> retrieveCommunicationIds (const db_model::HashedId& recipient) override;
    /**
     * Tries to remove the communication object defined by its id and sender from the database.
     * If no data row for the specified id and sender has been found the method returns no value.
     * If the data row has been deleted a tuple consisting of the id as the first element and
     * the optional value of the column "received"a as the second element is returned.
     * If "received" does not have a value this means that the communication has not been
     * received by the recipient.
     */
    std::tuple<std::optional<Uuid>, std::optional<model::Timestamp>>
    deleteCommunication (const Uuid& communicationId, const db_model::HashedId& sender) override;

    void deleteCommunicationsForTask (const model::PrescriptionId& taskId) override;
    void markCommunicationsAsRetrieved (
        const std::vector<Uuid>& communicationIds,
        const model::Timestamp& retrieved,
        const db_model::HashedId& recipient) override;
   // Test-only methods.
    void deleteTask (const model::PrescriptionId& taskId);
    void deleteAuditEvent(const Uuid& eventId);
    void deleteAuditEventDataForTask (const model::PrescriptionId& taskId);

    [[nodiscard]]
    std::optional<db_model::Blob> retrieveSaltForAccount(const db_model::HashedId& accountId,
                                                         db_model::MasterKeyType masterKeyType,
                                                         BlobId blobId) override;

    /// @returns true if the salt has actually been inserted
    [[nodiscard]]
    std::optional<db_model::Blob>
    insertOrReturnAccountSalt(const db_model::HashedId& accountId,
                              db_model::MasterKeyType masterKeyType,
                              BlobId blobId,
                              const db_model::Blob& salt) override;

    void storeConsent(const db_model::HashedKvnr& kvnr, const model::Timestamp& creationTime) override;
    std::optional<model::Timestamp> retrieveConsentDateTime(const db_model::HashedKvnr & kvnr) override;
    [[nodiscard]] bool clearConsent(const db_model::HashedKvnr & kvnr) override;

    void storeChargeInformation(const ::db_model::ChargeItem& chargeItem, ::db_model::HashedKvnr kvnr) override;
    void updateChargeInformation(const ::db_model::ChargeItem& chargeItem) override;

    std::vector<db_model::ChargeItem>
    retrieveAllChargeItemsForInsurant(const db_model::HashedKvnr& requestingInsurant,
                                      const std::optional<UrlArguments>& search) const override;

    ::db_model::ChargeItem retrieveChargeInformation(const model::PrescriptionId& id) const override;
    ::db_model::ChargeItem retrieveChargeInformationForUpdate(const model::PrescriptionId& id) const override;

    void deleteChargeInformation(const model::PrescriptionId& id) override;
    void clearAllChargeInformation(const db_model::HashedKvnr& insurant) override;

    void clearAllChargeItemCommunications(const db_model::HashedKvnr& insurant) override;
    void deleteCommunicationsForChargeItem(const model::PrescriptionId& id) override;

    uint64_t countChargeInformationForInsurant(const db_model::HashedKvnr& kvnr,
                                               const std::optional<UrlArguments>& search) override;

    bool isBlobUsed(BlobId blobId) const;

    void insertTask(const model::Task& task,
                const std::optional<std::string>& medicationDispense = std::nullopt,
                const std::optional<std::string>& healthCareProviderPrescription = std::nullopt,
                const std::optional<std::string>& receipt = std::nullopt);

    void insertAuditEvent(const model::AuditEvent& auditEvent,
                          model::AuditEventId id);

    std::optional<db_model::Blob> retrieveMedicationDispenseSalt(const model::PrescriptionId& taskId);

private:
    [[nodiscard]]
    std::optional<db_model::EncryptedBlob> optionalEncrypt(const SafeString& key,
                                                                 const std::optional<std::string_view>& data,
                                                                 Compression::DictionaryUse dictUse
                                                                    = Compression::DictionaryUse::Default_json) const;

    SafeString getMedicationDispenseKey(const db_model::HashedKvnr& kvnr, BlobId blobId);

    SafeString getAuditEventKey(const db_model::HashedKvnr& kvnr, BlobId& blobId);

    std::mutex mutex;

    std::list<db_model::AuditData> mAuditEventData;

    std::map<std::string, CmacKey> mCmac;
    MockAccountTable mAccounts;
    std::map<model::PrescriptionType, MockTaskTable> mTasks;
    MockCommunicationTable mCommunications;
    MockConsentTable mConsents;
    MockChargeItemTable mChargeItems;
    KeyDerivation mDerivation;
    HsmPool& mHsmPool;
    DataBaseCodec mCodec;
};

#endif
