/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/mock/MockDatabaseProxy.hxx"
#include "erp/model/Consent.hxx"
#include "shared/crypto/CMAC.hxx"
#include "shared/hsm/HsmClient.hxx"
#include "test/mock/MockDatabase.hxx"

thread_local bool MockDatabaseProxy::TransactionMonitor::inProgress = false;

MockDatabaseProxy::MockDatabaseProxy(MockDatabase& database)
    : mDatabase(database)
{
}

CmacKey MockDatabaseProxy::acquireCmac(const date::sys_days& validDate, const CmacKeyCategory& cmacType,
                                       RandomSource& randomSource)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.acquireCmac(validDate, cmacType, randomSource);
}

void MockDatabaseProxy::activateTask(const model::PrescriptionId& taskId, const db_model::EncryptedBlob& encryptedKvnr,
                                     const db_model::HashedKvnr& hashedKvnr, model::Task::Status taskStatus,
                                     const model::Timestamp& lastModified, const model::Timestamp& expiryDate,
                                     const model::Timestamp& acceptDate,
                                     const db_model::EncryptedBlob& healthCareProviderPrescription,
                                     const db_model::EncryptedBlob& doctorIdentity,
                                     const model::Timestamp& lastStatusUpdate)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.activateTask(taskId, encryptedKvnr, hashedKvnr, taskStatus, lastModified, expiryDate, acceptDate,
                           healthCareProviderPrescription, doctorIdentity, lastStatusUpdate);
}

void MockDatabaseProxy::updateTaskReceipt(const model::PrescriptionId& taskId, const model::Task::Status& taskStatus,
                                          const model::Timestamp& lastModified, const db_model::EncryptedBlob& receipt,
                                          const db_model::EncryptedBlob& pharmacyIdentity,
                                          const model::Timestamp& lastStatusUpdate)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.updateTaskReceipt(taskId, taskStatus, lastModified, receipt, pharmacyIdentity, lastStatusUpdate);
}

void MockDatabaseProxy::commitTransaction()
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.commitTransaction();
}

void MockDatabaseProxy::closeConnection()
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.closeConnection();
}

bool MockDatabaseProxy::isCommitted() const
{
    return ! transactionMonitor.inProgress;
}

std::string MockDatabaseProxy::retrieveSchemaVersion()
{
    return mDatabase.retrieveSchemaVersion();
}

void MockDatabaseProxy::healthCheck()
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.healthCheck();
}

uint64_t MockDatabaseProxy::countRepresentativeCommunications(const db_model::HashedKvnr& insurantA,
                                                              const db_model::HashedKvnr& insurantB,
                                                              const model::PrescriptionId& prescriptionId)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.countRepresentativeCommunications(insurantA, insurantB, prescriptionId);
}

std::tuple<model::PrescriptionId, model::Timestamp>
MockDatabaseProxy::createTask(model::PrescriptionType prescriptionType, model::Task::Status taskStatus,
                              const model::Timestamp& lastUpdated, const model::Timestamp& created,
                              const model::Timestamp& lastStatusUpdate)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.createTask(prescriptionType, taskStatus, lastUpdated, created, lastStatusUpdate);
}

void MockDatabaseProxy::deleteCommunicationsForTask(const model::PrescriptionId& taskId)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.deleteCommunicationsForTask(taskId);
}
std::optional<Uuid>
MockDatabaseProxy::insertCommunication(const model::PrescriptionId& prescriptionId, const model::Timestamp& timeSent,
                                       const model::Communication::MessageType messageType,
                                       const db_model::HashedId& sender, const db_model::HashedId& recipient,
                                       BlobId senderBlobId, const db_model::EncryptedBlob& messageForSender,
                                       BlobId recipientBlobId, const db_model::EncryptedBlob& messageForRecipient)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.insertCommunication(prescriptionId, timeSent, messageType, sender, recipient, senderBlobId,
                                         messageForSender, recipientBlobId, messageForRecipient);
}

void MockDatabaseProxy::markCommunicationsAsRetrieved(const std::vector<Uuid>& communicationIds,
                                                      const model::Timestamp& retrieved,
                                                      const db_model::HashedId& recipient)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.markCommunicationsAsRetrieved(communicationIds, retrieved, recipient);
}

std::vector<db_model::MedicationDispense>
MockDatabaseProxy::retrieveAllMedicationDispenses(const db_model::HashedKvnr& kvnr,
                                                  const std::optional<model::PrescriptionId>& prescriptionId,
                                                  const std::optional<UrlArguments>& search)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveAllMedicationDispenses(kvnr, prescriptionId, search);
}

std::vector<db_model::Task> MockDatabaseProxy::retrieveAllTasksForPatient(const db_model::HashedKvnr& kvnrHashed,
                                                                          const std::optional<UrlArguments>& search)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveAllTasksForPatient(kvnrHashed, search);
}

std::vector<db_model::Task>
MockDatabaseProxy::retrieveAll160TasksWithAccessCode(const db_model::HashedKvnr& kvnrHashed,
                                                     const std::optional<UrlArguments>& search)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveAll160TasksWithAccessCode(kvnrHashed, search);
}

uint64_t MockDatabaseProxy::countAllTasksForPatient(const db_model::HashedKvnr& kvnr,
                                                    const std::optional<UrlArguments>& search)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.countAllTasksForPatient(kvnr, search);
}

uint64_t MockDatabaseProxy::countAll160Tasks(const db_model::HashedKvnr& kvnr,
                                             const std::optional<UrlArguments>& search)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.countAll160Tasks(kvnr, search);
}

std::vector<db_model::AuditData>
MockDatabaseProxy::retrieveAuditEventData(const db_model::HashedKvnr& kvnr, const std::optional<Uuid>& id,
                                          const std::optional<model::PrescriptionId>& prescriptionId,
                                          const std::optional<UrlArguments>& search)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveAuditEventData(kvnr, id, prescriptionId, search);
}

std::vector<Uuid> MockDatabaseProxy::retrieveCommunicationIds(const db_model::HashedId& recipient)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveCommunicationIds(recipient);
}

bool MockDatabaseProxy::existCommunication(const Uuid& communicationId)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.existCommunication(communicationId);
}

std::vector<db_model::Communication>
MockDatabaseProxy::retrieveCommunications(const db_model::HashedId& user, const std::optional<Uuid>& communicationId,
                                          const std::optional<UrlArguments>& search)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveCommunications(user, communicationId, search);
}

uint64_t MockDatabaseProxy::countCommunications(const db_model::HashedId& user,
                                                const std::optional<UrlArguments>& search)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.countCommunications(user, search);
}

std::optional<db_model::Task> MockDatabaseProxy::retrieveTaskAndPrescription(const model::PrescriptionId& taskId)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveTaskAndPrescription(taskId);
}

std::optional<db_model::Task>
MockDatabaseProxy::retrieveTaskWithSecretAndPrescription(const model::PrescriptionId& taskId)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveTaskWithSecretAndPrescription(taskId);
}

std::optional<db_model::Task>
MockDatabaseProxy::retrieveTaskAndPrescriptionAndReceipt(const model::PrescriptionId& taskId)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveTaskAndPrescriptionAndReceipt(taskId);
}

std::optional<db_model::Task> MockDatabaseProxy::retrieveTaskAndReceipt(const model::PrescriptionId& taskId)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveTaskAndReceipt(taskId);
}

std::optional<db_model::Task> MockDatabaseProxy::retrieveTaskForUpdate(const model::PrescriptionId& taskId)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveTaskForUpdate(taskId);
}

::std::optional<::db_model::Task>
MockDatabaseProxy::retrieveTaskForUpdateAndPrescription(const ::model::PrescriptionId& taskId)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveTaskForUpdateAndPrescription(taskId);
}

std::string MockDatabaseProxy::storeAuditEventData(db_model::AuditData& auditData)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.storeAuditEventData(auditData);
}

void MockDatabaseProxy::updateTask(const model::PrescriptionId& taskId, const db_model::EncryptedBlob& accessCode,
                                   uint32_t blobId, const db_model::Blob& salt)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.updateTask(taskId, accessCode, blobId, salt);
}

void MockDatabaseProxy::updateTaskDeleteMedicationDispense(const model::PrescriptionId& taskId,
                                                           const model::Timestamp& lastModified)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.updateTaskDeleteMedicationDispense(taskId, lastModified);
}

void MockDatabaseProxy::updateTaskClearPersonalData(const model::PrescriptionId& taskId, model::Task::Status taskStatus,
                                                    const model::Timestamp& lastModified,
                                                    const model::Timestamp& lastStatusUpdate)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.updateTaskClearPersonalData(taskId, taskStatus, lastModified, lastStatusUpdate);
}

void MockDatabaseProxy::updateTaskMedicationDispense(
    const model::PrescriptionId& taskId, const model::Timestamp& lastModified,
    const model::Timestamp& lastMedicationDispense, const db_model::EncryptedBlob& medicationDispense,
    BlobId medicationDispenseBlobId, const db_model::HashedTelematikId& telematicId,
    const model::Timestamp& whenHandedOver, const std::optional<model::Timestamp>& whenPrepared,
    const db_model::Blob& medicationDispenseSalt)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.updateTaskMedicationDispense(taskId, lastModified, lastMedicationDispense, medicationDispense,
                                           medicationDispenseBlobId, telematicId, whenHandedOver, whenPrepared,
                                           medicationDispenseSalt);
}

void MockDatabaseProxy::updateTaskMedicationDispenseReceipt(
    const model::PrescriptionId& taskId, const model::Task::Status& taskStatus, const model::Timestamp& lastModified,
    const db_model::EncryptedBlob& medicationDispense, BlobId medicationDispenseBlobId,
    const db_model::HashedTelematikId& telematicId, const model::Timestamp& whenHandedOver,
    const std::optional<model::Timestamp>& whenPrepared, const db_model::EncryptedBlob& receipt,
    const model::Timestamp& lastMedicationDispense, const db_model::Blob& medicationDispenseSalt,
    const db_model::EncryptedBlob& pharmacyIdentity,
    const model::Timestamp& lastStatusUpdate)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.updateTaskMedicationDispenseReceipt(
        taskId, taskStatus, lastModified, medicationDispense, medicationDispenseBlobId, telematicId, whenHandedOver,
        whenPrepared, receipt, lastMedicationDispense, medicationDispenseSalt, pharmacyIdentity, lastStatusUpdate);
}

void MockDatabaseProxy::updateTaskStatusAndSecret(const model::PrescriptionId& taskId, model::Task::Status status,
                                                  const model::Timestamp& lastModifiedDate,
                                                  const std::optional<db_model::EncryptedBlob>& secret,
                                                  const std::optional<db_model::EncryptedBlob>& owner,
                                                  const model::Timestamp& lastStatusUpdate)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.updateTaskStatusAndSecret(taskId, status, lastModifiedDate, secret, owner, lastStatusUpdate);
}
std::tuple<BlobId, db_model::Blob, model::Timestamp>
MockDatabaseProxy::getTaskKeyData(const model::PrescriptionId& taskId)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.getTaskKeyData(taskId);
}

std::tuple<std::optional<Uuid>, std::optional<model::Timestamp>>
MockDatabaseProxy::deleteCommunication(const Uuid& communicationId, const db_model::HashedId& sender)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.deleteCommunication(communicationId, sender);
}

std::optional<db_model::Blob> MockDatabaseProxy::insertOrReturnAccountSalt(const db_model::HashedId& accountId,
                                                                           db_model::MasterKeyType masterKeyType,
                                                                           BlobId blobId, const db_model::Blob& salt)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.insertOrReturnAccountSalt(accountId, masterKeyType, blobId, salt);
}

std::optional<db_model::Blob> MockDatabaseProxy::retrieveSaltForAccount(const db_model::HashedId& accountId,
                                                                        db_model::MasterKeyType masterKeyType,
                                                                        BlobId blobId)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveSaltForAccount(accountId, masterKeyType, blobId);
}

void MockDatabaseProxy::storeConsent(const db_model::HashedKvnr& kvnr, const model::Timestamp& creationTime)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.storeConsent(kvnr, creationTime);
}

std::optional<model::Timestamp> MockDatabaseProxy::retrieveConsentDateTime(const db_model::HashedKvnr& kvnr)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveConsentDateTime(kvnr);
}

bool MockDatabaseProxy::clearConsent(const db_model::HashedKvnr& kvnr)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.clearConsent(kvnr);
}

void MockDatabaseProxy::storeChargeInformation(const ::db_model::ChargeItem& chargeItem, ::db_model::HashedKvnr kvnr)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.storeChargeInformation(chargeItem, kvnr);
}

void MockDatabaseProxy::updateChargeInformation(const ::db_model::ChargeItem& chargeItem)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.updateChargeInformation(chargeItem);
}

::std::vector<::db_model::ChargeItem>
MockDatabaseProxy::retrieveAllChargeItemsForInsurant(const ::db_model::HashedKvnr& requestingInsurant,
                                                     const ::std::optional<::UrlArguments>& search) const
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveAllChargeItemsForInsurant(requestingInsurant, search);
}

::db_model::ChargeItem MockDatabaseProxy::retrieveChargeInformation(const ::model::PrescriptionId& id) const
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveChargeInformation(id);
}

::db_model::ChargeItem MockDatabaseProxy::retrieveChargeInformationForUpdate(const ::model::PrescriptionId& id) const
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.retrieveChargeInformationForUpdate(id);
}

void MockDatabaseProxy::deleteChargeInformation(const ::model::PrescriptionId& id)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.deleteChargeInformation(id);
}

void MockDatabaseProxy::clearAllChargeInformation(const ::db_model::HashedKvnr& insurant)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.clearAllChargeInformation(insurant);
}

void MockDatabaseProxy::clearAllChargeItemCommunications(const ::db_model::HashedKvnr& insurant)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.clearAllChargeItemCommunications(insurant);
}

void MockDatabaseProxy::deleteCommunicationsForChargeItem(const model::PrescriptionId& id)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.deleteCommunicationsForChargeItem(id);
}

uint64_t MockDatabaseProxy::countChargeInformationForInsurant(const ::db_model::HashedKvnr& insurant,
                                                              const ::std::optional<::UrlArguments>& search)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.countChargeInformationForInsurant(insurant, search);
}

bool MockDatabaseProxy::isBlobUsed(BlobId blobId) const
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    return mDatabase.isBlobUsed(blobId);
}

void MockDatabaseProxy::deleteTask(const model::PrescriptionId& taskId)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.deleteTask(taskId);
}

void MockDatabaseProxy::deleteAuditEvent(const Uuid& eventId)
{
    Expect3(transactionMonitor.inProgress, "transaction already committed!", std::logic_error);
    mDatabase.deleteAuditEvent(eventId);
}

std::optional<db_model::Blob> MockDatabaseProxy::retrieveMedicationDispenseSalt(const model::PrescriptionId& taskId)
{
    return mDatabase.retrieveMedicationDispenseSalt(taskId);
}

MockDatabaseProxy::TransactionMonitor::TransactionMonitor()
{
    Expect3(! inProgress, "Created new MockDatabaseProxy instance while there is another uncommitted instance",
            std::logic_error);
    inProgress = true;
}

MockDatabaseProxy::TransactionMonitor::~TransactionMonitor() noexcept
{
    inProgress = false;
}
