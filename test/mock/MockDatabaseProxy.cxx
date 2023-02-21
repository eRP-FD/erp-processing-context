/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/mock/MockDatabaseProxy.hxx"
#include "test/mock/MockDatabase.hxx"

#include "erp/crypto/CMAC.hxx"
#include "erp/hsm/HsmClient.hxx"
#include "erp/model/Consent.hxx"


MockDatabaseProxy::MockDatabaseProxy(MockDatabase& database)
    : mDatabase(database)
{
}

CmacKey MockDatabaseProxy::acquireCmac(const date::sys_days& validDate, const CmacKeyCategory& cmacType, RandomSource& randomSource)
{
    return mDatabase.acquireCmac(validDate, cmacType, randomSource);
}

void MockDatabaseProxy::activateTask(const model::PrescriptionId& taskId,
                                     const db_model::EncryptedBlob& encryptedKvnr,
                                     const db_model::HashedKvnr& hashedKvnr,
                                     model::Task::Status taskStatus,
                                     const model::Timestamp& lastModified,
                                     const model::Timestamp& expiryDate,
                                     const model::Timestamp& acceptDate,
                                     const db_model::EncryptedBlob& healthCareProviderPrescription)
{
    mDatabase.activateTask(taskId, encryptedKvnr, hashedKvnr, taskStatus, lastModified, expiryDate, acceptDate,
                           healthCareProviderPrescription);
}

void MockDatabaseProxy::commitTransaction()
{
    mDatabase.commitTransaction();
}

void MockDatabaseProxy::closeConnection()
{
    mDatabase.closeConnection();
}

bool MockDatabaseProxy::isCommitted() const
{
    return mDatabase.isCommitted();
}

void MockDatabaseProxy::healthCheck()
{
    mDatabase.healthCheck();
}

uint64_t MockDatabaseProxy::countRepresentativeCommunications(const db_model::HashedKvnr& insurantA,
                                                              const db_model::HashedKvnr& insurantB,
                                                              const model::PrescriptionId& prescriptionId)
{
    return mDatabase.countRepresentativeCommunications(insurantA, insurantB, prescriptionId);
}

std::tuple<model::PrescriptionId, model::Timestamp> MockDatabaseProxy::createTask(model::PrescriptionType prescriptionType,
                                                                                  model::Task::Status taskStatus,
                                                                                  const model::Timestamp& lastUpdated,
                                                                                  const model::Timestamp& created)
{
    return mDatabase.createTask(prescriptionType, taskStatus, lastUpdated, created);
}

void MockDatabaseProxy::deleteCommunicationsForTask(const model::PrescriptionId& taskId)
{
    mDatabase.deleteCommunicationsForTask(taskId);
}
std::optional<Uuid> MockDatabaseProxy::insertCommunication(const model::PrescriptionId& prescriptionId,
                                                           const model::Timestamp& timeSent,
                                                           const model::Communication::MessageType messageType,
                                                           const db_model::HashedId& sender,
                                                           const db_model::HashedId& recipient,
                                                           BlobId senderBlobId,
                                                           const db_model::EncryptedBlob& messageForSender,
                                                           BlobId recipientBlobId,
                                                           const db_model::EncryptedBlob& messageForRecipient)
{
    return mDatabase.insertCommunication(prescriptionId, timeSent, messageType, sender, recipient,
                                         senderBlobId, messageForSender, recipientBlobId, messageForRecipient);
}

void MockDatabaseProxy::markCommunicationsAsRetrieved(const std::vector<Uuid>& communicationIds,
                                                      const model::Timestamp& retrieved,
                                                      const db_model::HashedId& recipient)
{
    return mDatabase.markCommunicationsAsRetrieved(communicationIds, retrieved, recipient);
}

std::tuple<std::vector<db_model::MedicationDispense>, bool>
MockDatabaseProxy::retrieveAllMedicationDispenses(const db_model::HashedKvnr& kvnr,
                                                  const std::optional<model::PrescriptionId>& prescriptionId,
                                                  const std::optional<UrlArguments>& search)
{
    return mDatabase.retrieveAllMedicationDispenses(kvnr, prescriptionId, search);
}

std::vector<db_model::Task> MockDatabaseProxy::retrieveAllTasksForPatient(const db_model::HashedKvnr& kvnrHashed,
                                                                          const std::optional<UrlArguments>& search)
{
    return mDatabase.retrieveAllTasksForPatient(kvnrHashed, search);
}

uint64_t MockDatabaseProxy::countAllTasksForPatient(const db_model::HashedKvnr& kvnr,
                                                    const std::optional<UrlArguments>& search)
{
    return mDatabase.countAllTasksForPatient(kvnr, search);
}

std::vector<db_model::AuditData>
MockDatabaseProxy::retrieveAuditEventData(const db_model::HashedKvnr& kvnr, const std::optional<Uuid>& id,
                                          const std::optional<model::PrescriptionId>& prescriptionId,
                                          const std::optional<UrlArguments>& search)
{
    return mDatabase.retrieveAuditEventData(kvnr, id, prescriptionId, search);
}

uint64_t MockDatabaseProxy::countAuditEventData(const db_model::HashedKvnr& kvnr,
                                                const std::optional<UrlArguments>& search)
{
    return mDatabase.countAuditEventData(kvnr, search);
}

std::vector<Uuid> MockDatabaseProxy::retrieveCommunicationIds(const db_model::HashedId& recipient)
{
    return mDatabase.retrieveCommunicationIds(recipient);
}

bool MockDatabaseProxy::existCommunication(const Uuid& communicationId)
{
    return mDatabase.existCommunication(communicationId);
}

std::vector<db_model::Communication> MockDatabaseProxy::retrieveCommunications(const db_model::HashedId& user,
                                                                            const std::optional<Uuid>& communicationId,
                                                                            const std::optional<UrlArguments>& search)
{
    return mDatabase.retrieveCommunications(user, communicationId, search);
}

uint64_t MockDatabaseProxy::countCommunications(const db_model::HashedId& user,
                                                const std::optional<UrlArguments>& search)
{
    return mDatabase.countCommunications(user, search);
}

std::optional<db_model::Task> MockDatabaseProxy::retrieveTaskAndPrescription(const model::PrescriptionId& taskId)
{
    return mDatabase.retrieveTaskAndPrescription(taskId);
}

std::optional<db_model::Task> MockDatabaseProxy::retrieveTaskAndPrescriptionAndReceipt(const model::PrescriptionId& taskId)
{
    return mDatabase.retrieveTaskAndPrescriptionAndReceipt(taskId);
}

std::optional<db_model::Task> MockDatabaseProxy::retrieveTaskAndReceipt(const model::PrescriptionId& taskId)
{
    return mDatabase.retrieveTaskAndReceipt(taskId);
}

std::optional<db_model::Task> MockDatabaseProxy::retrieveTaskForUpdate(const model::PrescriptionId& taskId)
{
    return mDatabase.retrieveTaskForUpdate(taskId);
}

::std::optional<::db_model::Task>
MockDatabaseProxy::retrieveTaskForUpdateAndPrescription(const ::model::PrescriptionId& taskId)
{
    return mDatabase.retrieveTaskForUpdateAndPrescription(taskId);
}

std::string MockDatabaseProxy::storeAuditEventData(db_model::AuditData& auditData)
{
    return mDatabase.storeAuditEventData(auditData);
}

void MockDatabaseProxy::updateTask(const model::PrescriptionId& taskId,
                                   const db_model::EncryptedBlob& accessCode,
                                   uint32_t blobId,
                                   const db_model::Blob& salt)
{
    mDatabase.updateTask(taskId, accessCode, blobId, salt);
}

void MockDatabaseProxy::updateTaskClearPersonalData(const model::PrescriptionId& taskId,
                                                    model::Task::Status taskStatus,
                                                    const model::Timestamp& lastModified)
{
    mDatabase.updateTaskClearPersonalData(taskId, taskStatus, lastModified);
}

void MockDatabaseProxy::updateTaskMedicationDispenseReceipt(const model::PrescriptionId& taskId,
                                                            const model::Task::Status& taskStatus,
                                                            const model::Timestamp& lastModified,
                                                            const db_model::EncryptedBlob& medicationDispense,
                                                            BlobId medicationDispenseBlobId,
                                                            const db_model::HashedTelematikId& telematicId,
                                                            const model::Timestamp& whenHandedOver,
                                                            const std::optional<model::Timestamp>& whenPrepared,
                                                            const db_model::EncryptedBlob& receipt)
{
    mDatabase.updateTaskMedicationDispenseReceipt(taskId, taskStatus, lastModified, medicationDispense,
                                                  medicationDispenseBlobId, telematicId,
                                                  whenHandedOver, whenPrepared, receipt);
}

void MockDatabaseProxy::updateTaskStatusAndSecret(const model::PrescriptionId& taskId,
                                                  model::Task::Status status,
                                                  const model::Timestamp& lastModifiedDate,
                                                  const std::optional<db_model::EncryptedBlob>& secret)
{
    mDatabase.updateTaskStatusAndSecret(taskId, status, lastModifiedDate, secret);
}
std::tuple<BlobId, db_model::Blob, model::Timestamp> MockDatabaseProxy::getTaskKeyData(const model::PrescriptionId& taskId)
{
    return mDatabase.getTaskKeyData(taskId);
}

std::tuple<std::optional<Uuid>, std::optional<model::Timestamp>>
MockDatabaseProxy::deleteCommunication(const Uuid& communicationId, const db_model::HashedId& sender)
{
    return mDatabase.deleteCommunication(communicationId, sender);
}

std::optional<db_model::Blob> MockDatabaseProxy::insertOrReturnAccountSalt(const db_model::HashedId& accountId,
                                                                           db_model::MasterKeyType masterKeyType,
                                                                           BlobId blobId,
                                                                           const db_model::Blob& salt)
{
    return mDatabase.insertOrReturnAccountSalt(accountId, masterKeyType, blobId, salt);
}

std::optional<db_model::Blob> MockDatabaseProxy::retrieveSaltForAccount(const db_model::HashedId& accountId,
                                                                        db_model::MasterKeyType masterKeyType,
                                                                        BlobId blobId)
{
    return mDatabase.retrieveSaltForAccount(accountId, masterKeyType, blobId);
}

void MockDatabaseProxy::storeConsent(const db_model::HashedKvnr& kvnr, const model::Timestamp& creationTime)
{
    mDatabase.storeConsent(kvnr, creationTime);
}

std::optional<model::Timestamp> MockDatabaseProxy::retrieveConsentDateTime(const db_model::HashedKvnr& kvnr)
{
    return mDatabase.retrieveConsentDateTime(kvnr);
}

bool MockDatabaseProxy::clearConsent(const db_model::HashedKvnr& kvnr)
{
    return mDatabase.clearConsent(kvnr);
}

void MockDatabaseProxy::storeChargeInformation(const ::db_model::ChargeItem& chargeItem, ::db_model::HashedKvnr kvnr)
{
    return mDatabase.storeChargeInformation(chargeItem, kvnr);
}

void MockDatabaseProxy::updateChargeInformation(const ::db_model::ChargeItem& chargeItem)
{
    return mDatabase.updateChargeInformation(chargeItem);
}

::std::vector<::db_model::ChargeItem>
MockDatabaseProxy::retrieveAllChargeItemsForInsurant(const ::db_model::HashedKvnr& requestingInsurant,
                                                     const ::std::optional<::UrlArguments>& search) const
{
    return mDatabase.retrieveAllChargeItemsForInsurant(requestingInsurant, search);
}

::db_model::ChargeItem MockDatabaseProxy::retrieveChargeInformation(const ::model::PrescriptionId& id) const
{
    return mDatabase.retrieveChargeInformation(id);
}

::db_model::ChargeItem MockDatabaseProxy::retrieveChargeInformationForUpdate(const ::model::PrescriptionId& id) const
{
    return mDatabase.retrieveChargeInformationForUpdate(id);
}

void MockDatabaseProxy::deleteChargeInformation(const ::model::PrescriptionId& id)
{
    mDatabase.deleteChargeInformation(id);
}

void MockDatabaseProxy::clearAllChargeInformation(const ::db_model::HashedKvnr& insurant)
{
    mDatabase.clearAllChargeInformation(insurant);
}

void MockDatabaseProxy::clearAllChargeItemCommunications(const ::db_model::HashedKvnr& insurant)
{
    mDatabase.clearAllChargeItemCommunications(insurant);
}

void MockDatabaseProxy::deleteCommunicationsForChargeItem(const model::PrescriptionId& id)
{
    mDatabase.deleteCommunicationsForChargeItem(id);
}

uint64_t MockDatabaseProxy::countChargeInformationForInsurant(const ::db_model::HashedKvnr& insurant,
                                                              const ::std::optional<::UrlArguments>& search)
{
    return mDatabase.countChargeInformationForInsurant(insurant, search);
}

bool MockDatabaseProxy::isBlobUsed(BlobId blobId) const
{
    return mDatabase.isBlobUsed(blobId);
}

void MockDatabaseProxy::deleteTask(const model::PrescriptionId& taskId)
{
    mDatabase.deleteTask(taskId);
}

void MockDatabaseProxy::deleteAuditEvent(const Uuid& eventId)
{
    mDatabase.deleteAuditEvent(eventId);
}
