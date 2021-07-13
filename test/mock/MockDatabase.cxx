#include "test/mock/MockDatabase.hxx"
#include "test_config.h"

#include "erp/compression/ZStd.hxx"
#include "erp/crypto/CMAC.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/DatabaseModel.hxx"
#include "erp/hsm/HsmClient.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Uuid.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/SafeString.hxx"
#include "erp/util/TLog.hxx"

#include "test/mock/TestUrlArguments.hxx"

#include <tuple>

namespace {
std::shared_ptr<Compression> compressionInstance()
{
    static auto theCompressionInstance =
        std::make_shared<ZStd>(Configuration::instance().getStringValue(ConfigurationKey::ZSTD_DICTIONARY_DIR));
    return theCompressionInstance;
}

template <typename T>
auto tryOptional(const model::Task& task, T (model::Task::*member)() const) -> std::optional<std::remove_reference_t<T>>
{
   try
    {
        return std::make_optional((task.*member)());
    }
    catch (const model::ModelException&)
    {
        return std::nullopt;
    }
}
} // anonymous namespace



std::unique_ptr<Database> MockDatabase::createMockDatabase(HsmPool& hsmPool, KeyDerivation& keyDerivation)
{
    return std::make_unique<DatabaseFrontend>(std::make_unique<MockDatabase>(hsmPool), hsmPool, keyDerivation);
}
using namespace model;



MockDatabase::MockDatabase(HsmPool& hsmPool)
    : mAccounts{}
    , mTasks{mAccounts}
    , mCommunications{mAccounts}
    , mDerivation{hsmPool}
    , mHsmPool{hsmPool}
    , mCodec{compressionInstance()}
{

}

std::tuple<BlobId, db_model::Blob, model::Timestamp>
MockDatabase::getTaskKeyData(const model::PrescriptionId& taskId)
{
    return mTasks.getTaskKeyData(taskId);
}

std::optional<db_model::Task> MockDatabase::retrieveTaskBasics(const model::PrescriptionId& taskId)
{
    return mTasks.retrieveTaskBasics(taskId);
}

std::vector<db_model::Task> MockDatabase::retrieveAllTasksForPatient(const db_model::HashedKvnr& kvnrHashed,
                                                                     const std::optional<UrlArguments>& search)
{
    return mTasks.retrieveAllTasksForPatient(kvnrHashed, search);
}

uint64_t MockDatabase::countAllTasksForPatient(const db_model::HashedKvnr& kvnr,
                                               const std::optional<UrlArguments>& search)
{
    return mTasks.countAllTasksForPatient(kvnr, search);
}

std::vector<db_model::MedicationDispense>
MockDatabase::retrieveAllMedicationDispenses(const db_model::HashedKvnr& kvnr,
                                             const std::optional<model::PrescriptionId>& prescriptionId,
                                             const std::optional<UrlArguments>& search)
{
    return mTasks.retrieveAllMedicationDispenses(kvnr, prescriptionId, search);
}

uint64_t MockDatabase::countAllMedicationDispenses(const db_model::HashedKvnr& kvnr,
                                                   const std::optional<UrlArguments>& search)
{
    return mTasks.countAllMedicationDispenses(kvnr, search);
}

std::optional<Uuid> MockDatabase::insertCommunication(
    const model::PrescriptionId& prescriptionId,
    const model::Timestamp& timeSent,
    const model::Communication::MessageType messageType,
    const db_model::HashedId& sender,
    const db_model::HashedId& recipient,
    const std::optional<model::Timestamp>& timeReceived,
    BlobId senderBlobId,
    const db_model::EncryptedBlob& messageForSender,
    BlobId recipientBlobId,
    const db_model::EncryptedBlob& messageForRecipient)
{
    return mCommunications.insertCommunication(prescriptionId, timeSent, messageType, sender, recipient, timeReceived,
                                               senderBlobId, messageForSender, recipientBlobId, messageForRecipient);
}

uint64_t MockDatabase::countRepresentativeCommunications(
    const db_model::HashedKvnr& insurantA,
    const db_model::HashedKvnr& insurantB,
    const PrescriptionId& prescriptionId)
{
    return mCommunications.countRepresentativeCommunications(insurantA, insurantB, prescriptionId);
}


bool MockDatabase::existCommunication(const Uuid& communicationId)
{
    return mCommunications.existCommunication(communicationId);
}


std::vector<db_model::Communication> MockDatabase::retrieveCommunications (
    const db_model::HashedId& recipient,
    const std::optional<Uuid>& communicationId,
    const std::optional<UrlArguments>& search)
{
    return mCommunications.retrieveCommunications(recipient, communicationId, search);
}


uint64_t MockDatabase::countCommunications(
    const db_model::HashedId& recipient,
    const std::optional<UrlArguments>& search)
{
    return mCommunications.countCommunications(recipient, search);
}


std::vector<Uuid> MockDatabase::retrieveCommunicationIds(const db_model::HashedId& recipient)
{
    return mCommunications.retrieveCommunicationIds(recipient);
}


std::tuple<std::optional<Uuid>, std::optional<Timestamp>>
MockDatabase::deleteCommunication(const Uuid& communicationId, const db_model::HashedId& sender)
{
    return mCommunications.deleteCommunication(communicationId, sender);
}

void MockDatabase::markCommunicationsAsRetrieved (
    const std::vector<Uuid>& communicationIds,
    const Timestamp& retrieved,
    const db_model::HashedId& recipient)
{
    mCommunications.markCommunicationsAsRetrieved(communicationIds, retrieved, recipient);
}

void MockDatabase::deleteCommunicationsForTask (const model::PrescriptionId& taskId)
{
    mCommunications.deleteCommunicationsForTask(taskId);
}

CmacKey MockDatabase::acquireCmac(const date::sys_days& validDate, RandomSource& randomSource)
{
    std::lock_guard lock(mutex);
    auto key = mCmac.find(validDate);
    if (key != mCmac.end())
    {
        return key->second;
    }
    auto newKey = CmacKey::randomKey(randomSource);
    mCmac.emplace(validDate, newKey);
    return newKey;
}

std::optional<db_model::EncryptedBlob> MockDatabase::optionalEncrypt(const SafeString& key,
                                                                           const std::optional<std::string_view>& data,
                                                                           Compression::DictionaryUse dictUse) const
{
    return data?std::make_optional(mCodec.encode(*data, key, dictUse)):std::nullopt;
}


SafeString MockDatabase::getMedicationDispenseKey(const db_model::HashedKvnr& kvnr,
                                                  std::optional<BlobId> blobId)
{
    if (!blobId)
    {
        blobId = mHsmPool.acquire().session().getLatestTaskPersistenceId();
    }
    auto salt = mAccounts.getSalt(kvnr, db_model::MasterKeyType::medicationDispense, *blobId);
    if (salt)
    {
        return mDerivation.medicationDispenseKey(kvnr, *blobId, *salt);
    }
    auto [key, derivationData] = mDerivation.initialMedicationDispenseKey(kvnr);
    db_model::Blob saltBlob{derivationData.salt.begin(), derivationData.salt.end()};
    (void)mAccounts.insertOrReturnAccountSalt(kvnr, db_model::MasterKeyType::medicationDispense,
                                              derivationData.blobId, std::move(saltBlob));
    return std::move(key);
}


SafeString MockDatabase::getAuditEventKey(const db_model::HashedKvnr& kvnr, BlobId& blobId)
{
    auto salt = mAccounts.getSalt(kvnr, db_model::MasterKeyType::auditevent, blobId);
    if (salt)
    {
        return mDerivation.auditEventKey(kvnr, blobId, *salt);
    }
    auto [key, derivationData] = mDerivation.initialAuditEventKey(kvnr);
    db_model::Blob saltBlob{derivationData.salt.begin(), derivationData.salt.end()};
    (void) mAccounts.insertOrReturnAccountSalt(kvnr, db_model::MasterKeyType::auditevent, derivationData.blobId,
                                               std::move(saltBlob));
    blobId = derivationData.blobId;
    return std::move(key);
}


void MockDatabase::insertTask(const model::Task& task,
                              const std::optional<std::string>& medicationDispense,
                              const std::optional<std::string>& healthCareProviderPrescription)
{
    auto& newRow = mTasks.createRow(task.prescriptionId(), task.lastModifiedDate(), task.authoredOn());
    auto [taskKey, derivationData] = mDerivation.initialTaskKey(task.prescriptionId(), task.authoredOn());
    newRow.kvnr = optionalEncrypt(taskKey, task.kvnr());
    newRow.kvnrHashed =
        task.kvnr()
            ? std::make_optional(mDerivation.hashKvnr(*task.kvnr()))
            : std::nullopt;
    newRow.expiryDate = tryOptional(task, &model::Task::expiryDate);
    newRow.acceptDate = tryOptional(task, &model::Task::acceptDate);
    newRow.status = task.status();
    newRow.taskKeyBlobId = derivationData.blobId;
    newRow.salt = db_model::Blob{derivationData.salt.begin(), derivationData.salt.end()};
    newRow.accessCode = optionalEncrypt(taskKey, tryOptional(task, &model::Task::accessCode));
    newRow.secret = optionalEncrypt(taskKey, task.secret());
    if (medicationDispense)
    {
        Expect3(newRow.kvnrHashed, "KVNR required to encrypt MedicationDispense", std::logic_error);
        auto key = getMedicationDispenseKey(*newRow.kvnrHashed, std::nullopt);
        newRow.medicationDispenseBundle = mCodec.encode(*medicationDispense, key,
                                                                Compression::DictionaryUse::Default_json);
    }
    newRow.healthcareProviderPrescription = optionalEncrypt(taskKey, healthCareProviderPrescription);
    TVLOG(1) << "insertTask: " << task.prescriptionId().toString();
}

void MockDatabase::insertAuditEvent(const model::AuditEvent& auditEvent,
                                    model::AuditEventId id)
{
    const std::string sourceObserverReference = std::string(auditEvent.sourceObserverReference());
    const auto deviceId = std::stoi(sourceObserverReference.substr(sourceObserverReference.rfind('/')+1));
    const auto hashedKvnr = mDerivation.hashKvnr(auditEvent.entityName());
    auto blobId = mHsmPool.acquire().session().getLatestAuditLogPersistenceId();
    auto key = getAuditEventKey(hashedKvnr, blobId);
    mAuditEventData.emplace_back(
        auditEvent.agentType(),
        id,
        *optionalEncrypt(
            key, model::AuditMetaData(auditEvent.agentName(), std::get<1>(auditEvent.agentWho())).serializeToJsonString()),
        auditEvent.action(),
        hashedKvnr,
        gsl::narrow_cast<int16_t>(deviceId),
        model::PrescriptionId::fromString(auditEvent.entityDescription()),
        blobId);
    mAuditEventData.back().id = std::string(auditEvent.id());
    mAuditEventData.back().recorded = auditEvent.recorded();
}

void MockDatabase::fillWithStaticData ()
{
    const auto dataPath(std::string{ TEST_DATA_DIR } + "/EndpointHandlerTest");
    auto task1 = model::Task::fromJson(FileHelper::readFileAsString(dataPath + "/task1.json"));
    auto task2 = model::Task::fromJson(FileHelper::readFileAsString(dataPath + "/task2.json"));
    auto task3 = model::Task::fromJson(FileHelper::readFileAsString(dataPath + "/task3.json"));
    auto task4 = model::Task::fromJson(FileHelper::readFileAsString(dataPath + "/task4.json"));
    task4.setHealthCarePrescriptionUuid();
    auto task5 = model::Task::fromJson(FileHelper::readFileAsString(dataPath + "/task5.json"));
    auto task6 = model::Task::fromJson(FileHelper::readFileAsString(dataPath + "/task6.json"));
    auto task7 = model::Task::fromJson(FileHelper::readFileAsString(dataPath + "/task7.json"));

    const auto healthCarePrescriptionUuid4 = task4.healthCarePrescriptionUuid().value();
    const auto healthCarePrescriptionBundle4 =
        model::Binary(healthCarePrescriptionUuid4, FileHelper::readFileAsString(dataPath + "/kbv_bundle.xml.p7s"))
        .serializeToJsonString();

    const auto medicationDispense1Content = FileHelper::readFileAsString(dataPath + "/medication_dispense1.json");

    insertTask(task1, medicationDispense1Content);
    insertTask(task2);
    insertTask(task3);
    insertTask(task4, std::nullopt, healthCarePrescriptionBundle4);
    insertTask(task5);
    insertTask(task6);
    insertTask(task7);

    // add static audit event entries
    insertAuditEvent(
        model::AuditEvent::fromJson(FileHelper::readFileAsString(dataPath + "/audit_event.json")),
        model::AuditEventId::POST_Task_activate);
    insertAuditEvent(
        model::AuditEvent::fromJson(FileHelper::readFileAsString(dataPath + "/audit_event_delete_task.json")),
        model::AuditEventId::Task_delete_expired_id);
}

std::tuple<model::PrescriptionId, model::Timestamp> MockDatabase::createTask(model::Task::Status taskStatus,
                                                                             const model::Timestamp& lastUpdated,
                                                                             const model::Timestamp& created)
{
    return mTasks.createTask(taskStatus, lastUpdated, created);
}

void MockDatabase::activateTask(const model::PrescriptionId& taskId,
                                const db_model::EncryptedBlob& encryptedKvnr,
                                const db_model::HashedKvnr& hashedKvnr,
                                model::Task::Status taskStatus,
                                const model::Timestamp& lastModified,
                                const model::Timestamp& expiryDate,
                                const model::Timestamp& acceptDate,
                                const db_model::EncryptedBlob& healthCareProviderPrescription)
{
    mTasks.activateTask(taskId, encryptedKvnr, hashedKvnr, taskStatus, lastModified, expiryDate, acceptDate,
                        healthCareProviderPrescription);
}

void MockDatabase::updateTask(const model::PrescriptionId& taskId,
                              const db_model::EncryptedBlob& accessCode,
                              BlobId blobId,
                              const db_model::Blob& salt)
{
    mTasks.updateTask(taskId, accessCode, blobId, salt);
}

void MockDatabase::updateTaskStatusAndSecret(const model::PrescriptionId& taskId,
                                             model::Task::Status taskStatus,
                                             const model::Timestamp& lastModifiedDate,
                                             const std::optional<db_model::EncryptedBlob>& taskSecret)
{
    return mTasks.updateTaskStatusAndSecret(taskId, taskStatus, lastModifiedDate, taskSecret);
}

void MockDatabase::updateTaskMedicationDispenseReceipt(const model::PrescriptionId& taskId,
                                                       const model::Task::Status& taskStatus,
                                                       const model::Timestamp& lastModified,
                                                       const db_model::EncryptedBlob& medicationDispense,
                                                       BlobId medicationDispenseBlobId,
                                                       const db_model::HashedTelematikId& telematicId,
                                                       const model::Timestamp& whenHandedOver,
                                                       const std::optional<model::Timestamp>& whenPrepared,
                                                       const db_model::EncryptedBlob& taskReceipt)
{
    mTasks.updateTaskMedicationDispenseReceipt(taskId, taskStatus, lastModified, medicationDispense,
                                               medicationDispenseBlobId, telematicId,
                                               whenHandedOver, whenPrepared, taskReceipt);
}

void MockDatabase::updateTaskClearPersonalData(const model::PrescriptionId& taskId, model::Task::Status taskStatus,
                                               const model::Timestamp& lastModified)
{
    mTasks.updateTaskClearPersonalData(taskId, taskStatus, lastModified);
}

std::string MockDatabase::storeAuditEventData(db_model::AuditData& auditData)
{
    auto id = Uuid().toString();
    auditData.id = id;
    auditData.recorded = model::Timestamp::now();
    mAuditEventData.emplace_back(auditData);
    return id;
}

std::vector<db_model::AuditData> MockDatabase::retrieveAuditEventData(
    const db_model::HashedKvnr& kvnr,
    const std::optional<Uuid>& id,
    const std::optional<model::PrescriptionId>& prescriptionId,
    const std::optional<UrlArguments>& search)
{
    std::vector<db_model::AuditData> allAuditData;
    for (const auto& auditData : mAuditEventData)
    {
        if(auditData.insurantKvnr == kvnr &&
           (!id.has_value() || id.value().toString() == auditData.id) &&
            (!prescriptionId.has_value() || prescriptionId == auditData.prescriptionId))
        {
            allAuditData.emplace_back(auditData);
        }
    }

    if (search.has_value())
        return TestUrlArguments(search.value()).apply(std::move(allAuditData));
    else
        return allAuditData;
}

uint64_t MockDatabase::countAuditEventData(
    const db_model::HashedKvnr& kvnr,
    const std::optional<UrlArguments>& search)
{
    auto result = retrieveAuditEventData(kvnr, {}, {}, {});  // get all data for kvnr;
    if (search.has_value())
        return TestUrlArguments(search.value()).applySearch(std::move(result)).size();
    else
        return result.size();
}

std::optional<db_model::Task> MockDatabase::retrieveTaskAndReceipt(const model::PrescriptionId& taskId)
{
    return mTasks.retrieveTaskAndReceipt(taskId);
}

std::optional<db_model::Task> MockDatabase::retrieveTaskAndPrescription(const model::PrescriptionId& taskId)
{
    return mTasks.retrieveTaskAndPrescription(taskId);
}

void MockDatabase::deleteTask (const model::PrescriptionId& taskId)
{
    mTasks.deleteTask(taskId);
}

void MockDatabase::deleteAuditEventDataForTask (const model::PrescriptionId& taskId)
{
    mAuditEventData.remove_if(
        [taskId](const auto& elem)
        {
            return elem.prescriptionId.has_value() && elem.prescriptionId->toDatabaseId() == taskId.toDatabaseId();
        });
}

void MockDatabase::commitTransaction()
{
}

void MockDatabase::closeConnection()
{
}

void MockDatabase::healthCheck()
{
}

std::optional<db_model::Task> MockDatabase::retrieveTaskForUpdate(const model::PrescriptionId& taskId)
{
    return mTasks.retrieveTaskBasics(taskId);
}

std::optional<db_model::Blob> MockDatabase::insertOrReturnAccountSalt(const db_model::HashedId& accountId,
                                                                      db_model::MasterKeyType masterKeyType,
                                                                      BlobId blobId,
                                                                      const db_model::Blob& salt)
{
    return mAccounts.insertOrReturnAccountSalt(accountId, masterKeyType, blobId, salt);
}

std::optional<db_model::Blob> MockDatabase::retrieveSaltForAccount(const db_model::HashedId& accountId,
                                                                   db_model::MasterKeyType masterKeyType,
                                                                   BlobId blobId)
{
    return mAccounts.getSalt(accountId, masterKeyType, blobId);
}
