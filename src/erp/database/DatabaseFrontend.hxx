/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_DATABASE_DATABASEFRONTEND_HXX
#define ERP_PROCESSING_CONTEXT_DATABASE_DATABASEFRONTEND_HXX

#include "erp/database/Database.hxx"
#include "erp/database/DatabaseCodec.hxx"
#include "erp/hsm/KeyDerivation.hxx"

#include <memory>
#include <tuple>

class DatabaseBackend;
class ErpVector;
class HsmPool;
struct OptionalDeriveKeyData;

namespace db_model
{
class Blob;
struct ChargeItem;
class Task;
}

namespace model
{
enum class PrescriptionType : uint8_t;
}

/// @brief Handles encryption and separtes the fields for the DatabaseBackend implementation
/// all accesses to the actual Database are performed through the Backend
class DatabaseFrontend : public Database
{
public:
    explicit DatabaseFrontend(std::unique_ptr<DatabaseBackend>&& backend, HsmPool& hsmPool, KeyDerivation& keyDerivation);
    ~DatabaseFrontend(void) override;

    void commitTransaction() override;
    void closeConnection() override;

    void healthCheck() override;

    [[nodiscard]] model::PrescriptionId storeTask(const model::Task& task) override;
    void updateTaskStatusAndSecret(const model::Task& task) override;
    void activateTask(const model::Task& task, const model::Binary& healthCareProviderPrescription) override;
    void updateTaskMedicationDispenseReceipt(const model::Task& task,
                                             const std::vector<model::MedicationDispense>& medicationDispenses,
                                             const model::ErxReceipt& receipt) override;
    void updateTaskClearPersonalData(const model::Task& task) override;

    [[nodiscard]] std::string storeAuditEventData(model::AuditData& auditData) override;
    [[nodiscard]] std::vector<model::AuditData>
    retrieveAuditEventData(const model::Kvnr& kvnr, const std::optional<Uuid>& id,
                           const std::optional<model::PrescriptionId>& prescriptionId,
                           const std::optional<UrlArguments>& search) override;
    [[nodiscard]] uint64_t countAuditEventData(const model::Kvnr& kvnr,
                                               const std::optional<UrlArguments>& search) override;

    [[nodiscard]] std::optional<model::Task> retrieveTaskForUpdate(const model::PrescriptionId& taskId) override;
    [[nodiscard]] ::std::tuple<::std::optional<::model::Task>, ::std::optional<::model::Binary>>
    retrieveTaskForUpdateAndPrescription(const ::model::PrescriptionId& taskId) override;

    [[nodiscard]] std::tuple<std::optional<model::Task>, std::optional<model::Bundle>>
    retrieveTaskAndReceipt(const model::PrescriptionId& taskId) override;
    [[nodiscard]] std::tuple<std::optional<model::Task>, std::optional<model::Binary>>
    retrieveTaskAndPrescription(const model::PrescriptionId& taskId) override;
    [[nodiscard]] std::tuple<std::optional<model::Task>, std::optional<model::Binary>, std::optional<model::Bundle>>
    retrieveTaskAndPrescriptionAndReceipt(const model::PrescriptionId& taskId) override;
    [[nodiscard]] std::vector<model::Task>
    retrieveAllTasksForPatient(const model::Kvnr& kvnr, const std::optional<UrlArguments>& search) override;
    [[nodiscard]] uint64_t
    countAllTasksForPatient (const model::Kvnr& kvnr, const std::optional<UrlArguments>& search) override;

    [[nodiscard]] std::tuple<std::vector<model::MedicationDispense>, bool>
    retrieveAllMedicationDispenses(const model::Kvnr& kvnr,
                                   const std::optional<UrlArguments>& search) override;
    [[nodiscard]] std::optional<model::MedicationDispense>
    retrieveMedicationDispense(const model::Kvnr& kvnr, const model::MedicationDispenseId& id) override;


    [[nodiscard]] CmacKey acquireCmac(const date::sys_days& validDate, const CmacKeyCategory& cmacType, RandomSource& randomSource) override;
    [[nodiscard]] std::optional<Uuid> insertCommunication(model::Communication& communication) override;
    [[nodiscard]] uint64_t countRepresentativeCommunications(const model::Kvnr& insurantA, const model::Kvnr& insurantB,
                                                             const model::PrescriptionId& prescriptionId) override;
    [[nodiscard]] bool existCommunication(const Uuid& communicationId) override;
    [[nodiscard]] std::vector<model::Communication>
    retrieveCommunications(const std::string& user, const std::optional<Uuid>& communicationId,
                           const std::optional<UrlArguments>& search) override;
    [[nodiscard]] uint64_t countCommunications(const std::string& user,
                                               const std::optional<UrlArguments>& search) override;
    [[nodiscard]] std::vector<Uuid> retrieveCommunicationIds(const std::string& recipient) override;
    [[nodiscard]] std::tuple<std::optional<Uuid>, std::optional<model::Timestamp>>
    deleteCommunication(const Uuid& communicationId, const std::string& sender) override;
    void markCommunicationsAsRetrieved(const std::vector<Uuid>& communicationIds, const model::Timestamp& retrieved,
                                       const std::string& recipient) override;
    void deleteCommunicationsForTask(const model::PrescriptionId& taskId) override;

    void storeConsent(const model::Consent& consent) override;
    std::optional<model::Consent> retrieveConsent(const model::Kvnr& kvnr) override;
    [[nodiscard]] bool clearConsent(const model::Kvnr& kvnr) override;

    void storeChargeInformation(const ::model::ChargeInformation& chargeInformation) override;
    void updateChargeInformation(const ::model::ChargeInformation& chargeInformation, const BlobId& blobId, const db_model::Blob& salt) override;

    std::vector<model::ChargeItem>
    retrieveAllChargeItemsForInsurant(const model::Kvnr& kvnr,
                                      const std::optional<UrlArguments>& search) const override;

    [[nodiscard]] ::model::ChargeInformation retrieveChargeInformation(const model::PrescriptionId& id) const override;
    [[nodiscard]] std::tuple<::model::ChargeInformation, BlobId, db_model::Blob>
    retrieveChargeInformationForUpdate(const model::PrescriptionId& id) const override;

    void deleteChargeInformation(const model::PrescriptionId& id) override;
    void clearAllChargeInformation(const model::Kvnr& kvnr) override;
    void clearAllChargeItemCommunications(const model::Kvnr& kvnr) override;
    void deleteCommunicationsForChargeItem(const model::PrescriptionId& id) override;

    [[nodiscard]] uint64_t countChargeInformationForInsurant(const model::Kvnr& kvnr,
                                                             const std::optional<UrlArguments>& search) override;

    [[nodiscard]] DatabaseBackend& getBackend() override;

private:
    static std::shared_ptr<Compression> compressionInstance();
    [[nodiscard]] model::Task getModelTask(const db_model::Task& dbTask,
                                           const std::optional<SafeString>& key = std::nullopt);
    [[nodiscard]] std::optional<model::Binary> getHealthcareProviderPrescription(const db_model::Task& dbTask,
                                                                                 const SafeString& key);
    [[nodiscard]] std::optional<model::Bundle> getReceipt(const db_model::Task& dbTask, const SafeString& key);

    [[nodiscard]] std::optional<model::Bundle>
    getDispenseItem(const std::optional<db_model::EncryptedBlob>& dbDispenseItem, const SafeString& key);

    [[nodiscard]] static ErpVector taskKeyDerivationData(const model::PrescriptionId& taskId,
                                                         const model::Timestamp& authoredOn);
    [[nodiscard]] SafeString taskKey(const model::PrescriptionId& taskId);
    [[nodiscard]] SafeString taskKey(const model::PrescriptionId& taskId, const model::Timestamp& authoredOn,
                                     const OptionalDeriveKeyData& secondCallData);
    [[nodiscard]] std::optional<SafeString> taskKey(const db_model::Task& dbTask);

    [[nodiscard]]
    std::tuple<SafeString, BlobId> communicationKeyAndId(const std::string_view& identity,
                                                                       const db_model::HashedId& identityHashed);

    [[nodiscard]] std::tuple<SafeString, BlobId> medicationDispenseKey(const db_model::HashedKvnr& hashedKvnr);
    [[nodiscard]] ::std::tuple<::SafeString, ::BlobId, ::db_model::Blob>
    chargeItemKey(const ::model::PrescriptionId& prescriptionId) const;
    [[nodiscard]] ::std::tuple<::SafeString, ::BlobId, ::db_model::Blob>
    chargeItemKey(const model::PrescriptionId& prescriptionId, const BlobId& blobId, const db_model::Blob& salt) const;

    [[nodiscard]] std::tuple<SafeString, BlobId> auditEventKey(const db_model::HashedKvnr& hashedKvnr);

    std::unique_ptr<DatabaseBackend> mBackend;
    HsmPool& mHsmPool;
    KeyDerivation& mDerivation;
    DataBaseCodec mCodec;
};


#endif
